// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;

    #[diplomat::enum_convert(core::cmp::Ordering)]
    #[diplomat::rust_link(core::cmp::Ordering, Enum)]
    pub enum ICU4XOrdering {
        Less = -1,
        Equal = 0,
        Greater = 1,
    }
}
