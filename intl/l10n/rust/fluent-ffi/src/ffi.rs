/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::builtins::FluentDateTimeOptionsRaw;
use fluent::types::FluentNumberCurrencyDisplayStyle;
use fluent::types::FluentNumberOptions;
use fluent::types::FluentNumberStyle;
use nsstring::nsCString;

pub enum RawNumberFormatter {}

#[repr(C)]
pub enum FluentNumberStyleRaw {
    Decimal,
    Currency,
    Percent,
}

impl From<FluentNumberStyle> for FluentNumberStyleRaw {
    fn from(input: FluentNumberStyle) -> Self {
        match input {
            FluentNumberStyle::Decimal => Self::Decimal,
            FluentNumberStyle::Currency => Self::Currency,
            FluentNumberStyle::Percent => Self::Percent,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum FluentNumberCurrencyDisplayStyleRaw {
    Symbol,
    Code,
    Name,
}

impl From<FluentNumberCurrencyDisplayStyle> for FluentNumberCurrencyDisplayStyleRaw {
    fn from(input: FluentNumberCurrencyDisplayStyle) -> Self {
        match input {
            FluentNumberCurrencyDisplayStyle::Symbol => Self::Symbol,
            FluentNumberCurrencyDisplayStyle::Code => Self::Code,
            FluentNumberCurrencyDisplayStyle::Name => Self::Name,
        }
    }
}

#[repr(C)]
pub struct FluentNumberOptionsRaw {
    pub style: FluentNumberStyleRaw,
    pub currency: nsCString,
    pub currency_display: FluentNumberCurrencyDisplayStyleRaw,
    pub use_grouping: bool,
    pub minimum_integer_digits: usize,
    pub minimum_fraction_digits: usize,
    pub maximum_fraction_digits: usize,
    pub minimum_significant_digits: isize,
    pub maximum_significant_digits: isize,
}

fn get_number_option(val: Option<usize>, min: usize, max: usize, default: usize) -> usize {
    if let Some(val) = val {
        if val >= min && val <= max {
            val
        } else {
            default
        }
    } else {
        default
    }
}

impl From<&FluentNumberOptions> for FluentNumberOptionsRaw {
    fn from(input: &FluentNumberOptions) -> Self {
        let currency: nsCString = if let Some(ref currency) = input.currency {
            currency.into()
        } else {
            nsCString::new()
        };

        //XXX: This should be fetched from currency table.
        let currency_digits = 2;

        // Keep it aligned with ECMA402 NumberFormat logic.
        let minfd_default = if input.style == FluentNumberStyle::Currency {
            currency_digits
        } else {
            0
        };
        let maxfd_default = match input.style {
            FluentNumberStyle::Decimal => 3,
            FluentNumberStyle::Currency => currency_digits,
            FluentNumberStyle::Percent => 0,
        };
        let minid = get_number_option(input.minimum_integer_digits, 1, 21, 1);
        let minfd = get_number_option(input.minimum_fraction_digits, 0, 20, minfd_default);
        let maxfd_actual_default = std::cmp::max(minfd, maxfd_default);
        let maxfd = get_number_option(
            input.maximum_fraction_digits,
            minfd,
            20,
            maxfd_actual_default,
        );

        let (minsd, maxsd) = if input.minimum_significant_digits.is_some()
            || input.maximum_significant_digits.is_some()
        {
            let minsd = get_number_option(input.minimum_significant_digits, 1, 21, 1);
            let maxsd = get_number_option(input.maximum_significant_digits, minsd, 21, 21);
            (minsd as isize, maxsd as isize)
        } else {
            (-1, -1)
        };

        Self {
            style: input.style.into(),
            currency,
            currency_display: input.currency_display.into(),
            use_grouping: input.use_grouping,
            minimum_integer_digits: minid,
            minimum_fraction_digits: minfd,
            maximum_fraction_digits: maxfd,
            minimum_significant_digits: minsd,
            maximum_significant_digits: maxsd,
        }
    }
}

pub enum RawDateTimeFormatter {}

extern "C" {
    pub fn FluentBuiltInNumberFormatterCreate(
        locale: &nsCString,
        options: &FluentNumberOptionsRaw,
    ) -> *mut RawNumberFormatter;
    pub fn FluentBuiltInNumberFormatterFormat(
        formatter: *const RawNumberFormatter,
        input: f64,
        out_count: &mut usize,
        out_capacity: &mut usize,
    ) -> *mut u8;
    pub fn FluentBuiltInNumberFormatterDestroy(formatter: *mut RawNumberFormatter);

    pub fn FluentBuiltInDateTimeFormatterCreate(
        locale: &nsCString,
        options: &FluentDateTimeOptionsRaw,
    ) -> *mut RawDateTimeFormatter;
    pub fn FluentBuiltInDateTimeFormatterFormat(
        formatter: *const RawDateTimeFormatter,
        input: f64,
        out_count: &mut u32,
    ) -> *mut u8;
    pub fn FluentBuiltInDateTimeFormatterDestroy(formatter: *mut RawDateTimeFormatter);
}
