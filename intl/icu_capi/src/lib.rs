// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/docs/process/boilerplate.md#library-annotations
#![no_std]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        // Exhaustiveness and Debug is not required for Diplomat types
    )
)]
#![allow(clippy::upper_case_acronyms)]
#![allow(clippy::needless_lifetimes)]
#![allow(clippy::result_unit_err)]

//! This crate contains the source of truth for the [Diplomat](https://github.com/rust-diplomat/diplomat)-generated
//! FFI bindings. This generates the C, C++, JavaScript, and TypeScript bindings. This crate also contains the `extern "C"`
//! FFI for ICU4X.
//!
//! While the types in this crate are public, APIs from this crate are *not intended to be used from Rust*
//! and as such this crate may unpredictably change its Rust API across compatible semver versions. The `extern "C"` APIs exposed
//! by this crate, while not directly documented, are stable within the same major semver version, as are the bindings exposed under
//! the `cpp/` and `js/` folders.
//!
//! This crate may still be explored for documentation on docs.rs, and there are generated language-specific docs available as well.
//! C++ has sphinx docs in `cpp/docs/`, and the header files also contain documentation comments. The JS version has sphinx docs under
//! `js/docs`, and the TypeScript sources in `js/include` are compatible with `tsdoc`.
//!
//! This crate is `no_std` and will not typically build as a staticlib on its own. If you wish to link to it you should prefer
//! using `icu_capi_staticlib`, or for more esoteric platforms you may write a shim crate depending on this crate that hooks in
//! an allocator and panic hook.
//!
//! More information on using ICU4X from C++ can be found in [our tutorial].
//!
//! [our tutorial]: https://github.com/unicode-org/icu4x/blob/main/docs/tutorials/cpp.md
// Renamed so you can't accidentally use it
#[cfg(target_arch = "wasm32")]
extern crate std as rust_std;

extern crate alloc;

// Common modules

pub mod common;
pub mod data_struct;
pub mod errors;
pub mod locale;
#[cfg(feature = "logging")]
pub mod logging;
#[macro_use]
pub mod provider;

// Components

#[cfg(feature = "icu_properties")]
pub mod bidi;
#[cfg(any(
    feature = "icu_datetime",
    feature = "icu_timezone",
    feature = "icu_calendar"
))]
pub mod calendar;
#[cfg(feature = "icu_casemap")]
pub mod casemap;
#[cfg(feature = "icu_collator")]
pub mod collator;
#[cfg(feature = "icu_properties")]
pub mod collections_sets;
#[cfg(any(
    feature = "icu_datetime",
    feature = "icu_timezone",
    feature = "icu_calendar"
))]
pub mod date;
#[cfg(any(
    feature = "icu_datetime",
    feature = "icu_timezone",
    feature = "icu_calendar"
))]
pub mod datetime;
#[cfg(feature = "icu_datetime")]
pub mod datetime_formatter;
#[cfg(feature = "icu_decimal")]
pub mod decimal;
#[cfg(feature = "icu_displaynames")]
pub mod displaynames;
#[cfg(feature = "icu_locid_transform")]
pub mod fallbacker;
#[cfg(feature = "icu_decimal")]
pub mod fixed_decimal;
#[cfg(any(feature = "icu_datetime", feature = "icu_timezone"))]
pub mod iana_bcp47_mapper;
#[cfg(feature = "icu_list")]
pub mod list;
#[cfg(feature = "icu_locid_transform")]
pub mod locale_directionality;
#[cfg(feature = "icu_locid_transform")]
pub mod locid_transform;
#[cfg(feature = "icu_timezone")]
pub mod metazone_calculator;
#[cfg(feature = "icu_normalizer")]
pub mod normalizer;
#[cfg(feature = "icu_normalizer")]
pub mod normalizer_properties;
#[cfg(feature = "icu_plurals")]
pub mod pluralrules;
#[cfg(feature = "icu_properties")]
pub mod properties_iter;
#[cfg(feature = "icu_properties")]
pub mod properties_maps;
#[cfg(feature = "icu_properties")]
pub mod properties_names;
#[cfg(feature = "icu_properties")]
pub mod properties_sets;
#[cfg(feature = "icu_properties")]
pub mod properties_unisets;
#[cfg(feature = "icu_properties")]
pub mod script;
#[cfg(feature = "icu_segmenter")]
pub mod segmenter_grapheme;
#[cfg(feature = "icu_segmenter")]
pub mod segmenter_line;
#[cfg(feature = "icu_segmenter")]
pub mod segmenter_sentence;
#[cfg(feature = "icu_segmenter")]
pub mod segmenter_word;
#[cfg(any(
    feature = "icu_datetime",
    feature = "icu_timezone",
    feature = "icu_calendar"
))]
pub mod time;
#[cfg(any(feature = "icu_datetime", feature = "icu_timezone"))]
pub mod timezone;
#[cfg(feature = "icu_datetime")]
pub mod timezone_formatter;
#[cfg(feature = "icu_calendar")]
pub mod week;
#[cfg(feature = "icu_datetime")]
pub mod zoned_formatter;
