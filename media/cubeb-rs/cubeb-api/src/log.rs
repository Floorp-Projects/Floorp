// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use LogLevel;
use sys;

#[macro_export]
macro_rules! log_internal {
    ($level: expr, $msg: expr) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level as i32 <= $crate::sys::g_cubeb_log_level {
                if let Some(log_callback) = $crate::sys::g_cubeb_log_callback {
                    let cstr = ::std::ffi::CString::new(concat!("%s:%d: ", $msg, "\n")).unwrap();
                    log_callback(cstr.as_ptr(), file!(), line!());
                }
            }
        }
    };
    ($level: expr, $fmt: expr, $($arg:tt)+) => {
        #[allow(unused_unsafe)]
        unsafe {
            if $level as i32 <= $crate::sys::g_cubeb_log_level {
                if let Some(log_callback) = $crate::sys::g_cubeb_log_callback {
                    let cstr = ::std::ffi::CString::new(concat!("%s:%d: ", $fmt, "\n")).unwrap();
                    log_callback(cstr.as_ptr(), file!(), line!(), $($arg)+);
                }
            }
        }
    }
}

#[macro_export]
macro_rules! logv {
    ($msg: expr) => (log_internal!($crate::LogLevel::Verbose, $msg));
    ($fmt: expr, $($arg: tt)+) => (log_internal!($crate::LogLevel::Verbose, $fmt, $($arg)*));
}

#[macro_export]
macro_rules! log {
    ($msg: expr) => (log_internal!($crate::LogLevel::Normal, $msg));
    ($fmt: expr, $($arg: tt)+) => (log_internal!($crate::LogLevel::Normal, $fmt, $($arg)*));
}

pub fn log_enabled() -> bool {
    unsafe { sys::g_cubeb_log_level != LogLevel::Disabled as _ }
}

#[cfg(test)]
mod tests {
    use super::*;

#[test]
fn test_normal_logging() {
    log!("This is log at normal level");
    log!("Formatted log %d", 1);
}

#[test]
fn test_verbose_logging() {
    logv!("This is a log at verbose level");
    logv!("Formatted log %d", 1);
}

#[test]
fn test_logging_disabled_by_default() {
    assert!(!log_enabled());
}
}
