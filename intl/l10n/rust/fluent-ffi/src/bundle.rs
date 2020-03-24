/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::builtins::{FluentDateTime, FluentDateTimeOptions, NumberFormat};
use fluent::resolve::ResolverError;
use fluent::{FluentArgs, FluentBundle, FluentError, FluentResource, FluentValue};
use fluent_pseudo::transform_dom;
use intl_memoizer::IntlLangMemoizer;
use nsstring::{nsACString, nsCString};
use std::borrow::Cow;
use std::collections::HashMap;
use std::mem;
use std::ops::{Deref, DerefMut};
use std::rc::Rc;
use thin_vec::ThinVec;
use unic_langid::LanguageIdentifier;

// Workaround for cbindgen limitation with types.
// See: https://github.com/eqrion/cbindgen/issues/488
pub struct FluentBundleRc(FluentBundle<Rc<FluentResource>>);

impl Deref for FluentBundleRc {
    type Target = FluentBundle<Rc<FluentResource>>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for FluentBundleRc {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl Into<FluentBundleRc> for FluentBundle<Rc<FluentResource>> {
    fn into(self) -> FluentBundleRc {
        FluentBundleRc(self)
    }
}

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
    transform_dom(s, true, false)
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

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_new(
    locales: &ThinVec<nsCString>,
    use_isolating: bool,
    pseudo_strategy: &nsACString,
) -> *mut FluentBundleRc {
    let mut langids = Vec::with_capacity(locales.len());

    for locale in locales {
        let langid: Result<LanguageIdentifier, _> = locale.to_string().parse();
        match langid {
            Ok(langid) => langids.push(langid),
            Err(_) => return std::ptr::null_mut(),
        }
    }
    let mut bundle = FluentBundle::new(&langids);
    bundle.set_use_isolating(use_isolating);

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

    if !pseudo_strategy.is_empty() {
        match &pseudo_strategy[..] {
            b"accented" => bundle.set_transform(Some(transform_accented)),
            b"bidi" => bundle.set_transform(Some(transform_bidi)),
            _ => bundle.set_transform(None),
        }
    }
    Box::into_raw(Box::new(bundle.into()))
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_get_locales(
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
pub unsafe extern "C" fn fluent_bundle_has_message(
    bundle: &FluentBundleRc,
    id: &nsACString,
) -> bool {
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
            *has_value = message.value.is_some();
            for key in message.attributes.keys() {
                attrs.push((*key).into());
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
    let arg_ids = arg_ids.iter().map(|id| id.to_string()).collect::<Vec<_>>();
    let args = convert_args(&arg_ids, arg_vals);

    let message = match bundle.get_message(id.as_str_unchecked()) {
        Some(message) => message,
        None => return false,
    };

    let pattern = if !attr.is_empty() {
        match message.attributes.get(attr.as_str_unchecked()) {
            Some(attr) => attr,
            None => return false,
        }
    } else {
        match message.value {
            Some(value) => value,
            None => return false,
        }
    };

    let mut errors = vec![];
    let value = bundle.format_pattern(pattern, args.as_ref(), &mut errors);
    ret_val.assign(value.as_bytes());
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

fn convert_args<'a>(ids: &'a [String], arg_vals: &'a [FluentArgument]) -> Option<FluentArgs<'a>> {
    debug_assert_eq!(ids.len(), arg_vals.len());

    if ids.is_empty() {
        return None;
    }

    let mut args = HashMap::with_capacity(arg_vals.len());
    for (id, val) in ids.iter().zip(arg_vals.iter()) {
        let val = match val {
            FluentArgument::Double_(d) => FluentValue::from(d),
            FluentArgument::String(s) => FluentValue::from(unsafe { (**s).to_string() }),
        };
        args.insert(id.as_str(), val);
    }
    Some(args)
}

fn append_fluent_errors_to_ret_errors(ret_errors: &mut ThinVec<nsCString>, errors: &[FluentError]) {
    for error in errors {
        match error {
            FluentError::ResolverError(ref err) => match err {
                ResolverError::Reference(ref s) => {
                    let error = format!("ReferenceError: {}", s);
                    ret_errors.push(error.into());
                }
                ResolverError::MissingDefault => {
                    let error = "RangeError: No default value for selector";
                    ret_errors.push(error.into());
                }
                ResolverError::Cyclic => {
                    let error =
                        "RangeError: Cyclic reference encountered while resolving a message";
                    ret_errors.push(error.into());
                }
                ResolverError::TooManyPlaceables => {
                    let error = "RangeError: Too many placeables in a message";
                    ret_errors.push(error.into());
                }
            },
            FluentError::Overriding { kind, id } => {
                let error = format!(
                    "OverrideError: An entry {} of type {} is already defined in this bundle",
                    id, kind
                );
                ret_errors.push(error.into());
            }
            FluentError::ParserError(pe) => {
                let error = format!(
                    "ParserError: Error of kind {:#?} in position: ({}, {})",
                    pe.kind, pe.pos.0, pe.pos.1
                );
                ret_errors.push(error.into());
            }
        }
    }
}
