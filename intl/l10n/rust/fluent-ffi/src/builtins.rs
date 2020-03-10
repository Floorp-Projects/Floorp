/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::ffi;
use fluent::types::FluentNumberOptions;
use intl_memoizer::Memoizable;
use nsstring::nsCString;
use std::ptr::NonNull;
use unic_langid::LanguageIdentifier;

pub struct NumberFormat {
    raw: NonNull<ffi::RawNumberFormatter>,
}

/**
 * According to http://userguide.icu-project.org/design, as long as we constrain
 * ourselves to const APIs ICU is const-correct.
 */
unsafe impl Send for NumberFormat {}
unsafe impl Sync for NumberFormat {}

impl NumberFormat {
    pub fn new(locale: LanguageIdentifier, options: &FluentNumberOptions) -> Self {
        let loc: String = locale.to_string();
        Self {
            raw: unsafe {
                NonNull::new_unchecked(ffi::FluentBuiltInNumberFormatterCreate(
                    &loc.into(),
                    &options.into(),
                ))
            },
        }
    }

    pub fn format(&self, input: f64) -> String {
        unsafe {
            let mut byte_count = 0;
            let buffer =
                ffi::FluentBuiltInNumberFormatterFormat(self.raw.as_ptr(), input, &mut byte_count);
            if buffer.is_null() {
                return String::new();
            }
            String::from_raw_parts(buffer, byte_count as usize, byte_count as usize)
        }
    }
}

impl Drop for NumberFormat {
    fn drop(&mut self) {
        unsafe { ffi::FluentBuiltInNumberFormatterDestroy(self.raw.as_ptr()) };
    }
}

impl Memoizable for NumberFormat {
    type Args = (FluentNumberOptions,);
    type Error = &'static str;
    fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
        Ok(NumberFormat::new(lang, &args.0))
    }
}
