// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use alloc::boxed::Box;

    #[diplomat::opaque]
    /// An object allowing control over the logging used
    #[diplomat::attr(dart, disable)]
    pub struct ICU4XLogger;

    impl ICU4XLogger {
        /// Initialize the logger using `simple_logger`
        ///
        /// Requires the `simple_logger` Cargo feature.
        ///
        /// Returns `false` if there was already a logger set.
        #[cfg(all(not(target_arch = "wasm32"), feature = "simple_logger"))]
        pub fn init_simple_logger() -> bool {
            simple_logger::init().is_ok()
        }

        /// Deprecated: since ICU4X 1.4, this now happens automatically if the `log` feature is enabled.
        #[cfg(target_arch = "wasm32")]
        pub fn init_console_logger() -> bool {
            false
        }
    }
}

// semver?
#[no_mangle]
#[cfg(target_arch = "wasm32")]
pub unsafe extern "C" fn icu4x_init() {}
