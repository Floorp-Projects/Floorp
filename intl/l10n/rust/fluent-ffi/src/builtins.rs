/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::ffi;
use fluent::types::{FluentNumberOptions, FluentType, FluentValue};
use fluent::FluentArgs;
use intl_memoizer::IntlLangMemoizer;
use intl_memoizer::Memoizable;
use nsstring::nsCString;
use std::borrow::Cow;
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
            let mut capacity = 0;
            let buffer = ffi::FluentBuiltInNumberFormatterFormat(
                self.raw.as_ptr(),
                input,
                &mut byte_count,
                &mut capacity,
            );
            if buffer.is_null() {
                return String::new();
            }
            String::from_raw_parts(buffer, byte_count, capacity)
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

#[repr(C)]
#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq)]
pub enum FluentDateTimeStyle {
    Full,
    Long,
    Medium,
    Short,
    None,
}

impl Default for FluentDateTimeStyle {
    fn default() -> Self {
        Self::None
    }
}

impl From<&str> for FluentDateTimeStyle {
    fn from(input: &str) -> Self {
        match input {
            "full" => Self::Full,
            "long" => Self::Long,
            "medium" => Self::Medium,
            "short" => Self::Short,
            _ => Self::None,
        }
    }
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum FluentDateTimeHourCycle {
    H24,
    H23,
    H12,
    H11,
    None,
}

impl Default for FluentDateTimeHourCycle {
    fn default() -> Self {
        Self::None
    }
}

impl From<&str> for FluentDateTimeHourCycle {
    fn from(input: &str) -> Self {
        match input {
            "h24" => Self::H24,
            "h23" => Self::H23,
            "h12" => Self::H12,
            "h11" => Self::H11,
            _ => Self::None,
        }
    }
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum FluentDateTimeTextComponent {
    Long,
    Short,
    Narrow,
    None,
}

impl Default for FluentDateTimeTextComponent {
    fn default() -> Self {
        Self::None
    }
}

impl From<&str> for FluentDateTimeTextComponent {
    fn from(input: &str) -> Self {
        match input {
            "long" => Self::Long,
            "short" => Self::Short,
            "narrow" => Self::Narrow,
            _ => Self::None,
        }
    }
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum FluentDateTimeNumericComponent {
    Numeric,
    TwoDigit,
    None,
}

impl Default for FluentDateTimeNumericComponent {
    fn default() -> Self {
        Self::None
    }
}

impl From<&str> for FluentDateTimeNumericComponent {
    fn from(input: &str) -> Self {
        match input {
            "numeric" => Self::Numeric,
            "2-digit" => Self::TwoDigit,
            _ => Self::None,
        }
    }
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum FluentDateTimeMonthComponent {
    Numeric,
    TwoDigit,
    Long,
    Short,
    Narrow,
    None,
}

impl Default for FluentDateTimeMonthComponent {
    fn default() -> Self {
        Self::None
    }
}

impl From<&str> for FluentDateTimeMonthComponent {
    fn from(input: &str) -> Self {
        match input {
            "numeric" => Self::Numeric,
            "2-digit" => Self::TwoDigit,
            "long" => Self::Long,
            "short" => Self::Short,
            "narrow" => Self::Narrow,
            _ => Self::None,
        }
    }
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum FluentDateTimeTimeZoneNameComponent {
    Long,
    Short,
    None,
}

impl Default for FluentDateTimeTimeZoneNameComponent {
    fn default() -> Self {
        Self::None
    }
}

impl From<&str> for FluentDateTimeTimeZoneNameComponent {
    fn from(input: &str) -> Self {
        match input {
            "long" => Self::Long,
            "short" => Self::Short,
            _ => Self::None,
        }
    }
}

#[repr(C)]
#[derive(Default, Debug, Clone, Hash, PartialEq, Eq)]
pub struct FluentDateTimeOptions {
    pub date_style: FluentDateTimeStyle,
    pub time_style: FluentDateTimeStyle,
    pub hour_cycle: FluentDateTimeHourCycle,
    pub weekday: FluentDateTimeTextComponent,
    pub era: FluentDateTimeTextComponent,
    pub year: FluentDateTimeNumericComponent,
    pub month: FluentDateTimeMonthComponent,
    pub day: FluentDateTimeNumericComponent,
    pub hour: FluentDateTimeNumericComponent,
    pub minute: FluentDateTimeNumericComponent,
    pub second: FluentDateTimeNumericComponent,
    pub time_zone_name: FluentDateTimeTimeZoneNameComponent,
}

impl FluentDateTimeOptions {
    pub fn merge(&mut self, opts: &FluentArgs) {
        for (key, value) in opts.iter() {
            match (key, value) {
                ("dateStyle", FluentValue::String(n)) => {
                    self.date_style = n.as_ref().into();
                }
                ("timeStyle", FluentValue::String(n)) => {
                    self.time_style = n.as_ref().into();
                }
                ("hourCycle", FluentValue::String(n)) => {
                    self.hour_cycle = n.as_ref().into();
                }
                ("weekday", FluentValue::String(n)) => {
                    self.weekday = n.as_ref().into();
                }
                ("era", FluentValue::String(n)) => {
                    self.era = n.as_ref().into();
                }
                ("year", FluentValue::String(n)) => {
                    self.year = n.as_ref().into();
                }
                ("month", FluentValue::String(n)) => {
                    self.month = n.as_ref().into();
                }
                ("day", FluentValue::String(n)) => {
                    self.day = n.as_ref().into();
                }
                ("hour", FluentValue::String(n)) => {
                    self.hour = n.as_ref().into();
                }
                ("minute", FluentValue::String(n)) => {
                    self.minute = n.as_ref().into();
                }
                ("second", FluentValue::String(n)) => {
                    self.second = n.as_ref().into();
                }
                ("timeZoneName", FluentValue::String(n)) => {
                    self.time_zone_name = n.as_ref().into();
                }
                _ => {}
            }
        }
    }
}

#[repr(C)]
pub struct FluentDateTimeOptionsRaw {
    pub date_style: FluentDateTimeStyle,
    pub time_style: FluentDateTimeStyle,
    pub skeleton: nsCString,
}

impl FluentDateTimeOptionsRaw {
    fn convert_options_to_skeleton(input: &FluentDateTimeOptions) -> String {
        let mut result = String::new();

        match input.weekday {
            FluentDateTimeTextComponent::Narrow => result.push_str("EEEEE"),
            FluentDateTimeTextComponent::Short => result.push_str("E"),
            FluentDateTimeTextComponent::Long => result.push_str("EEEE"),
            FluentDateTimeTextComponent::None => {}
        }
        match input.era {
            FluentDateTimeTextComponent::Narrow => result.push_str("GGGGG"),
            FluentDateTimeTextComponent::Short => result.push_str("G"),
            FluentDateTimeTextComponent::Long => result.push_str("GGGG"),
            FluentDateTimeTextComponent::None => {}
        }
        match input.year {
            FluentDateTimeNumericComponent::Numeric => result.push_str("y"),
            FluentDateTimeNumericComponent::TwoDigit => result.push_str("yy"),
            FluentDateTimeNumericComponent::None => {}
        }
        match input.month {
            FluentDateTimeMonthComponent::Numeric => result.push_str("M"),
            FluentDateTimeMonthComponent::TwoDigit => result.push_str("MM"),
            FluentDateTimeMonthComponent::Narrow => result.push_str("MMMMM"),
            FluentDateTimeMonthComponent::Short => result.push_str("MMM"),
            FluentDateTimeMonthComponent::Long => result.push_str("MMMM"),
            FluentDateTimeMonthComponent::None => {}
        }
        match input.day {
            FluentDateTimeNumericComponent::Numeric => result.push_str("d"),
            FluentDateTimeNumericComponent::TwoDigit => result.push_str("dd"),
            FluentDateTimeNumericComponent::None => {}
        }
        let hour_skeleton_char = match input.hour_cycle {
            FluentDateTimeHourCycle::H24 => 'H',
            FluentDateTimeHourCycle::H23 => 'H',
            FluentDateTimeHourCycle::H12 => 'h',
            FluentDateTimeHourCycle::H11 => 'h',
            FluentDateTimeHourCycle::None => 'j',
        };
        match input.hour {
            FluentDateTimeNumericComponent::Numeric => result.push(hour_skeleton_char),
            FluentDateTimeNumericComponent::TwoDigit => {
                result.push(hour_skeleton_char);
                result.push(hour_skeleton_char);
            }
            FluentDateTimeNumericComponent::None => {}
        }
        match input.minute {
            FluentDateTimeNumericComponent::Numeric => result.push_str("m"),
            FluentDateTimeNumericComponent::TwoDigit => result.push_str("mm"),
            FluentDateTimeNumericComponent::None => {}
        }
        match input.second {
            FluentDateTimeNumericComponent::Numeric => result.push_str("s"),
            FluentDateTimeNumericComponent::TwoDigit => result.push_str("ss"),
            FluentDateTimeNumericComponent::None => {}
        }
        match input.time_zone_name {
            FluentDateTimeTimeZoneNameComponent::Short => result.push_str("z"),
            FluentDateTimeTimeZoneNameComponent::Long => result.push_str("zzzz"),
            FluentDateTimeTimeZoneNameComponent::None => {}
        }
        result
    }
}

impl From<&FluentDateTimeOptions> for FluentDateTimeOptionsRaw {
    fn from(input: &FluentDateTimeOptions) -> Self {
        let skeleton = if input.date_style == FluentDateTimeStyle::None
            && input.time_style == FluentDateTimeStyle::None
        {
            Self::convert_options_to_skeleton(&input).into()
        } else {
            nsCString::new()
        };
        Self {
            date_style: input.date_style,
            time_style: input.time_style,
            skeleton,
        }
    }
}

#[derive(Debug, PartialEq, Clone)]
pub struct FluentDateTime {
    epoch: f64,
    options: FluentDateTimeOptions,
}

impl FluentType for FluentDateTime {
    fn duplicate(&self) -> Box<dyn FluentType + Send> {
        Box::new(self.clone())
    }
    fn as_string(&self, intls: &IntlLangMemoizer) -> Cow<'static, str> {
        let result = intls
            .with_try_get::<DateTimeFormat, _, _>((self.options.clone(),), |dtf| {
                dtf.format(self.epoch)
            })
            .expect("Failed to retrieve a DateTimeFormat instance.");
        result.into()
    }
    fn as_string_threadsafe(
        &self,
        _: &intl_memoizer::concurrent::IntlLangMemoizer,
    ) -> Cow<'static, str> {
        unimplemented!()
    }
}

impl std::fmt::Display for FluentDateTime {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "DATETIME: {}", self.epoch)
    }
}

impl FluentDateTime {
    pub fn new(epoch: f64, options: FluentDateTimeOptions) -> Self {
        Self { epoch, options }
    }
}

pub struct DateTimeFormat {
    raw: NonNull<ffi::RawDateTimeFormatter>,
}

/**
 * According to http://userguide.icu-project.org/design, as long as we constrain
 * ourselves to const APIs ICU is const-correct.
 */
unsafe impl Send for DateTimeFormat {}
unsafe impl Sync for DateTimeFormat {}

impl DateTimeFormat {
    pub fn new(locale: LanguageIdentifier, options: FluentDateTimeOptions) -> Self {
        // ICU needs null-termination here, otherwise we could use nsCStr.
        let loc: nsCString = locale.to_string().into();
        Self {
            raw: unsafe {
                NonNull::new_unchecked(ffi::FluentBuiltInDateTimeFormatterCreate(
                    &loc,
                    &(&options).into(),
                ))
            },
        }
    }

    pub fn format(&self, input: f64) -> String {
        unsafe {
            let mut byte_count = 0;
            let buffer = ffi::FluentBuiltInDateTimeFormatterFormat(
                self.raw.as_ptr(),
                input,
                &mut byte_count,
            );
            if buffer.is_null() {
                return String::new();
            }
            String::from_raw_parts(buffer, byte_count as usize, byte_count as usize)
        }
    }
}

impl Drop for DateTimeFormat {
    fn drop(&mut self) {
        unsafe { ffi::FluentBuiltInDateTimeFormatterDestroy(self.raw.as_ptr()) };
    }
}

impl Memoizable for DateTimeFormat {
    type Args = (FluentDateTimeOptions,);
    type Error = &'static str;
    fn construct(lang: LanguageIdentifier, args: Self::Args) -> Result<Self, Self::Error> {
        Ok(DateTimeFormat::new(lang, args.0))
    }
}
