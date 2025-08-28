#include "token.xakti.hpp"

void token::create( const name& issuer, const asset& maximum_supply ) {
    require_auth( get_self() );

    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "nama symbol tidak valid" );
    check( maximum_supply.is_valid(), "suplai maksimum tidak valid");
    check( maximum_supply.amount > 0, "suplai maksimum harus positif");

    Stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "token dengan symbol ini sudah ada" );

    statstable.emplace( get_self(), [&]( auto& s ) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply    = maximum_supply;
        s.issuer        = issuer;
    });
}

void token::issue( const name& to, const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "nama symbol tidak valid" );
    check( memo.size() <= 256, "memo tidak boleh lebih dari 256 karakter" );

    Stats statstable( get_self(), sym.code().raw() );
    auto itr = statstable.find( sym.code().raw() );
    check( itr != statstable.end(), "token dengan symbol ini tidak ditemukan, buat token dahulu" );
    check( to == itr->issuer, "token hanya bisa dicetak kepada akun yang membuat" );

    require_auth( itr->issuer );
    check( quantity.is_valid(), "quantity tidak valid" );
    check( quantity.amount > 0, "jumlah harus nilai positif" );

    check( quantity.symbol == itr->supply.symbol, "symbol precision tidak sama" );
    check( quantity.amount <= itr->max_supply.amount - itr->supply.amount, "jumlah melebihi suplai yang ada");

    statstable.modify( itr, same_payer, [&]( auto& s ) {
        s.supply += quantity;
    });

    add_balance( itr->issuer, quantity, itr->issuer );
}

void token::retire( const asset& quantity, const string& memo ) {
    auto sym = quantity.symbol;
    check( sym.is_valid(), "nama symbol tidak valid" );
    check( memo.size() <= 256, "memo tidak boleh lebih dari 256 karakter" );

    Stats statstable( get_self(), sym.code().raw() );
    auto itr = statstable.find( sym.code().raw() );
    check( itr != statstable.end(), "token dengan symbol ini tidak ditemukan" );

    require_auth( itr->issuer );
    check( quantity.is_valid(), "quantity tidak valid" );
    check( quantity.amount > 0, "jumlah harus nilai positif" );

    check( quantity.symbol == itr->supply.symbol, "symbol precision tidak sama" );

    statstable.modify( itr, same_payer, [&]( auto& s ) {
        s.supply -= quantity;
    });

    sub_balance( itr->issuer, quantity );
}

void token::transfer( const name& from, const name& to, const asset& quantity, const string& memo ) {
    check( from != to, "silakan transfer ke akun lain" );
    require_auth( from );
    check( is_account( to ), "akun penerima tidak ditemukan");

    auto sym = quantity.symbol.code();
    Stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "quantity tidak valid" );
    check( quantity.amount > 0, "jumlah transfer harus nilai positif" );
    check( quantity.symbol == st.supply.symbol, "symbol precision tidak sama" );
    check( memo.size() <= 256, "memo tidak boleh lebih dari 256 karakter" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer ) {
    require_auth( ram_payer );

    check( is_account( owner ), "akun tidak ditemukan" );

    auto sym_code_raw = symbol.code().raw();
    Stats statstable( get_self(), sym_code_raw );
    const auto& st = statstable.get( sym_code_raw, "symbol tidak ditemukan" );
    check( st.supply.symbol == symbol, "symbol precision tidak sama" );

    Accounts acnts( get_self(), owner.value );
    auto itr = acnts.find( sym_code_raw );
    if ( itr == acnts.end() ) {
        acnts.emplace( ram_payer, [&]( auto& a ) {
            a.balance = asset{0, symbol};
        });
    }
}

void token::close( const name& owner, const symbol& symbol ) {
    require_auth( owner );
    Accounts acnts( get_self(), owner.value );
    auto itr = acnts.find( symbol.code().raw() );
    check( itr != acnts.end(), "saldo sudah dihapus atau belum pernah dibuat" );
    check( itr->balance.amount == 0, "saldo masih ada" );
    acnts.erase( itr );
}

/* void token::closesupply( const symbol_code symcode ) {
    require_auth( get_self() );

    Stats _stats( get_self(), symcode.raw() );
    auto stats = _stats.require_find( symcode.raw(), "symbol code tidak ditemukan" );
    check( stats->supply.amount == 0, "suplai harus nol" );
    _stats.erase( stats );
} */

void token::add_balance( const name& owner, const asset& value, const name& ram_payer ) {
    Accounts to_acnts( get_self(), owner.value );
    auto to = to_acnts.find( value.symbol.code().raw() );
    if ( to == to_acnts.end() ) {
        to_acnts.emplace( ram_payer, [&]( auto& a ) {
            a.balance = value;
        });
    } else {
        to_acnts.modify( to, same_payer, [&]( auto& a ) {
            a.balance += value;
        });
    }
}

void token::sub_balance( const name& owner, const asset& value ) {
    Accounts from_acnts( get_self(), owner.value );

    const auto from = from_acnts.require_find( value.symbol.code().raw(), "belum punya saldo" );
    check( from->balance.amount >= value.amount, "saldo tidak cukup" );

    from_acnts.modify( from, owner, [&]( auto& a ) {
        a.balance -= value;
    });
}
