/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::builtins::{FluentDateTime, FluentDateTimeOptions, NumberFormat};
use cstr::cstr;
pub use fluent::{FluentArgs, FluentBundle, FluentError, FluentResource, FluentValue};
use fluent_pseudo::transform_dom;
pub use intl_memoizer::IntlLangMemoizer;
use nsstring::{nsACString, nsCString};
use std::borrow::Cow;
use std::ffi::CStr;
use std::mem;
use std::rc::Rc;
use thin_vec::ThinVec;
use unic_langid::LanguageIdentifier;
use xpcom::interfaces::nsIPrefBranch;

pub type FluentBundleRc = FluentBundle<Rc<FluentResource>>;

#[derive(Debug)]
#[repr(C, u8)]
pub enum FluentArgument {
    Double_(f64),
    String(*const nsCString),
}

fn transform_accented(s: &str) -> Cow<str> {
    transform_dom(s, false, true)
}

fn transform_bidi(s: &str) -> Cow<str> {
    transform_dom(s, false, false)
}

fn format_numbers(num: &FluentValue, intls: &IntlLangMemoizer) -> Option<String> {
    match num {
        FluentValue::Number(n) => {
            let result = intls
                .with_try_get::<NumberFormat, _, _>((n.options.clone(),), |nf| nf.format(n.value))
                .expect("Failed to retrieve a NumberFormat instance.");
            Some(result)
        }
        _ => None,
    }
}

fn get_string_pref(name: &CStr) -> Option<nsCString> {
    let mut value = nsCString::new();
    let prefs_service =
        xpcom::get_service::<nsIPrefBranch>(cstr!("@mozilla.org/preferences-service;1"))?;
    unsafe {
        prefs_service
            .GetCharPref(name.as_ptr(), &mut *value)
            .to_result()
            .ok()?;
    }
    Some(value)
}

fn get_bool_pref(name: &CStr) -> Option<bool> {
    let mut value = false;
    let prefs_service =
        xpcom::get_service::<nsIPrefBranch>(cstr!("@mozilla.org/preferences-service;1"))?;
    unsafe {
        prefs_service
            .GetBoolPref(name.as_ptr(), &mut value)
            .to_result()
            .ok()?;
    }
    Some(value)
}

pub fn adapt_bundle_for_gecko(bundle: &mut FluentBundleRc, pseudo_strategy: Option<&nsACString>) {
    bundle.set_formatter(Some(format_numbers));

    bundle
        .add_function("PLATFORM", |_args, _named_args| {
            if cfg!(target_os = "linux") {
                "linux".into()
            } else if cfg!(target_os = "windows") {
                "windows".into()
            } else if cfg!(target_os = "macos") {
                "macos".into()
            } else if cfg!(target_os = "android") {
                "android".into()
            } else {
                "other".into()
            }
        })
        .expect("Failed to add a function to the bundle.");
    bundle
        .add_function("NUMBER", |args, named| {
            if let Some(FluentValue::Number(n)) = args.get(0) {
                let mut num = n.clone();
                num.options.merge(named);
                FluentValue::Number(num)
            } else {
                FluentValue::None
            }
        })
        .expect("Failed to add a function to the bundle.");
    bundle
        .add_function("DATETIME", |args, named| {
            if let Some(FluentValue::Number(n)) = args.get(0) {
                let mut options = FluentDateTimeOptions::default();
                options.merge(&named);
                FluentValue::Custom(Box::new(FluentDateTime::new(n.value, options)))
            } else {
                FluentValue::None
            }
        })
        .expect("Failed to add a function to the bundle.");

    enum PseudoStrategy {
        Accented,
        Bidi,
        None,
    }
    // This is quirky because we can't coerce Option<&nsACString> and Option<nsCString>
    // into bytes easily without allocating.
    let strategy_kind = match pseudo_strategy.map(|s| &s[..]) {
        Some(b"accented") => PseudoStrategy::Accented,
        Some(b"bidi") => PseudoStrategy::Bidi,
        _ => {
            if let Some(pseudo_strategy) = get_string_pref(cstr!("intl.l10n.pseudo")) {
                match &pseudo_strategy[..] {
                    b"accented" => PseudoStrategy::Accented,
                    b"bidi" => PseudoStrategy::Bidi,
                    _ => PseudoStrategy::None,
                }
            } else {
                PseudoStrategy::None
            }
        }
    };
    match strategy_kind {
        PseudoStrategy::Accented => bundle.set_transform(Some(transform_accented)),
        PseudoStrategy::Bidi => bundle.set_transform(Some(transform_bidi)),
        PseudoStrategy::None => bundle.set_transform(None),
    }

    // Temporarily disable bidi isolation due to Microsoft not supporting FSI/PDI.
    // See bug 1439018 for details.
    let default_use_isolating = false;
    let use_isolating =
        get_bool_pref(cstr!("intl.l10n.enable-bidi-marks")).unwrap_or(default_use_isolating);
    bundle.set_use_isolating(use_isolating);
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_new_single(
    locale: &nsACString,
    use_isolating: bool,
    pseudo_strategy: &nsACString,
) -> *mut FluentBundleRc {
    // We can use as_str_unchecked because this string comes from WebIDL and is
    // guaranteed utf-8.
    let id = match locale.as_str_unchecked().parse::<LanguageIdentifier>() {
        Ok(id) => id,
        Err(..) => return std::ptr::null_mut(),
    };

    Box::into_raw(fluent_bundle_new_internal(
        &[id],
        use_isolating,
        pseudo_strategy,
    ))
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_new(
    locales: *const nsCString,
    locale_count: usize,
    use_isolating: bool,
    pseudo_strategy: &nsACString,
) -> *mut FluentBundleRc {
    let mut langids = Vec::with_capacity(locale_count);
    let locales = std::slice::from_raw_parts(locales, locale_count);
    for locale in locales {
        let id = match locale.as_str_unchecked().parse::<LanguageIdentifier>() {
            Ok(id) => id,
            Err(..) => return std::ptr::null_mut(),
        };
        langids.push(id);
    }

    Box::into_raw(fluent_bundle_new_internal(
        &langids,
        use_isolating,
        pseudo_strategy,
    ))
}

fn fluent_bundle_new_internal(
    langids: &[LanguageIdentifier],
    use_isolating: bool,
    pseudo_strategy: &nsACString,
) -> Box<FluentBundleRc> {
    let mut bundle = FluentBundle::new(langids.to_vec());
    bundle.set_use_isolating(use_isolating);

    bundle.set_formatter(Some(format_numbers));

    adapt_bundle_for_gecko(&mut bundle, Some(pseudo_strategy));

    Box::new(bundle)
}

#[no_mangle]
pub extern "C" fn fluent_bundle_get_locales(
    bundle: &FluentBundleRc,
    result: &mut ThinVec<nsCString>,
) {
    for locale in &bundle.locales {
        result.push(locale.to_string().as_str().into());
    }
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_destroy(bundle: *mut FluentBundleRc) {
    let _ = Box::from_raw(bundle);
}

#[no_mangle]
pub extern "C" fn fluent_bundle_has_message(bundle: &FluentBundleRc, id: &nsACString) -> bool {
    bundle.has_message(id.to_string().as_str())
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_get_message(
    bundle: &FluentBundleRc,
    id: &nsACString,
    has_value: &mut bool,
    attrs: &mut ThinVec<nsCString>,
) -> bool {
    match bundle.get_message(id.as_str_unchecked()) {
        Some(message) => {
            attrs.reserve(message.attributes().count());
            *has_value = message.value().is_some();
            for attr in message.attributes() {
                attrs.push(attr.id().into());
            }
            true
        }
        None => {
            *has_value = false;
            false
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_format_pattern(
    bundle: &FluentBundleRc,
    id: &nsACString,
    attr: &nsACString,
    arg_ids: &ThinVec<nsCString>,
    arg_vals: &ThinVec<FluentArgument>,
    ret_val: &mut nsACString,
    ret_errors: &mut ThinVec<nsCString>,
) -> bool {
    let args = convert_args(arg_ids, arg_vals);

    let message = match bundle.get_message(id.as_str_unchecked()) {
        Some(message) => message,
        None => return false,
    };

    let pattern = if !attr.is_empty() {
        match message.get_attribute(attr.as_str_unchecked()) {
            Some(attr) => attr.value(),
            None => return false,
        }
    } else {
        match message.value() {
            Some(value) => value,
            None => return false,
        }
    };

    let mut errors = vec![];
    bundle
        .write_pattern(ret_val, pattern, args.as_ref(), &mut errors)
        .expect("Failed to write to a nsCString.");
    append_fluent_errors_to_ret_errors(ret_errors, &errors);
    true
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_add_resource(
    bundle: &mut FluentBundleRc,
    r: &FluentResource,
    allow_overrides: bool,
    ret_errors: &mut ThinVec<nsCString>,
) {
    // we don't own the resource
    let r = mem::ManuallyDrop::new(Rc::from_raw(r));

    if allow_overrides {
        bundle.add_resource_overriding(Rc::clone(&r));
    } else if let Err(errors) = bundle.add_resource(Rc::clone(&r)) {
        append_fluent_errors_to_ret_errors(ret_errors, &errors);
    }
}

fn convert_args<'a>(
    arg_ids: &'a [nsCString],
    arg_vals: &'a [FluentArgument],
) -> Option<FluentArgs<'a>> {
    debug_assert_eq!(arg_ids.len(), arg_vals.len());

    if arg_ids.is_empty() {
        return None;
    }

    let mut args = FluentArgs::with_capacity(arg_ids.len());
    for (id, val) in arg_ids.iter().zip(arg_vals.iter()) {
        let val = match val {
            FluentArgument::Double_(d) => FluentValue::from(d),
            FluentArgument::String(s) => FluentValue::from(unsafe { (**s).to_string() }),
        };
        args.set(id.to_string(), val);
    }
    Some(args)
}

fn append_fluent_errors_to_ret_errors(ret_errors: &mut ThinVec<nsCString>, errors: &[FluentError]) {
    for error in errors {
        ret_errors.push(error.to_string().into());
    }
}
