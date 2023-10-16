// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

pub use bitflags::*;

// Copy of the macro from bitflags 1.3.2, with the implicit derives
// removed, because in 2.0, they're expected to be explicitly given
// in the macro invocation. And because bitflags 1.3.2 itself always
// adds an impl Debug, we need to remove #[derive(Debug)] from what
// is passed in, which is what the __impl_bitflags_remove_derive_debug
// macro does.
#[macro_export(local_inner_macros)]
macro_rules! bitflags {
    (
        $(#[$($outer:tt)+])*
        $vis:vis struct $BitFlags:ident: $T:ty {
            $(
                $(#[$inner:ident $($args:tt)*])*
                const $Flag:ident = $value:expr;
            )*
        }

        $($t:tt)*
    ) => {
        __impl_bitflags_remove_derive_debug! {
            $(#[$($outer)+])*

            $vis struct $BitFlags {
                bits: $T,
            }
        }

        __impl_bitflags! {
            $BitFlags: $T {
                $(
                    $(#[$inner $($args)*])*
                    $Flag = $value;
                )*
            }
        }

        impl $BitFlags {
            /// Convert from underlying bit representation, preserving all
            /// bits (even those not corresponding to a defined flag).
            #[inline]
            pub const fn from_bits_retain(bits: $T) -> Self {
                Self { bits }
            }
        }

        bitflags! {
            $($t)*
        }
    };
    () => {};
}

#[macro_export(local_inner_macros)]
#[doc(hidden)]
macro_rules! __impl_bitflags_remove_derive_debug {
    (
        $(@ $(#[$keep_outer:meta])* @)?
        #[derive(@ $($keep_derives:ident),+ @)]
        $(#[$($other_outer:tt)+])*
        $vis:vis struct $BitFlags:ident { $($t:tt)* }
    ) => {
        __impl_bitflags_remove_derive_debug! {
            @ $($(#[$keep_outer])*)? #[derive($($keep_derives),+)] @
            $(#[$($other_outer)+])*
            $vis struct $BitFlags { $($t)* }
        }
    };
    (
        $(@ $(#[$keep_outer:meta])* @)?
        #[derive($(@ $($keep_derives:ident),+ @)? Debug $(,$derives:ident)*)]
        $(#[$($other_outer:tt)+])*
        $vis:vis struct $BitFlags:ident { $($t:tt)* }
    ) => {
        __impl_bitflags_remove_derive_debug! {
            @ $($(#[$keep_outer])*)? @
            #[derive($(@ $($keep_derives),+ @)? $($derives),*)]
            $(#[$($other_outer)+])*
            $vis struct $BitFlags { $($t)* }
        }
    };
    (
        $(@ $(#[$keep_outer:meta])* @)?
        #[derive($(@ $($keep_derives:ident),+ @)? $derive:ident $(,$derives:ident)*)]
        $(#[$($other_outer:tt)+])*
        $vis:vis struct $BitFlags:ident { $($t:tt)* }
    ) => {
        __impl_bitflags_remove_derive_debug! {
            @ $($(#[$keep_outer])*)? @
            #[derive(@ $($($keep_derives,)+)? $derive @ $($derives),*)]
            $(#[$($other_outer)+])*
            $vis struct $BitFlags { $($t)* }
        }
    };
    (
        $(@ $(#[$keep_outer:meta])* @)?
        #[$outer:meta]
        $(#[$($other_outer:tt)+])*
        $vis:vis struct $BitFlags:ident { $($t:tt)* }
    ) => {
        __impl_bitflags_remove_derive_debug! {
            @ $($(#[$keep_outer])*)? #[$outer] @
            $(#[$($other_outer)+])*
            $vis struct $BitFlags { $($t)* }
        }
    };
    (
        @ $(#[$keep_outer:meta])* @
        $vis:vis struct $BitFlags:ident { $($t:tt)* }
    ) => {
        $(#[$keep_outer])*
        $vis struct $BitFlags { $($t)* }
    };
}
