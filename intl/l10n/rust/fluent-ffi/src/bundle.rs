use fluent::resolve::ResolverError;
use fluent::{FluentArgs, FluentBundle, FluentError, FluentResource, FluentValue};
use fluent_pseudo::transform_dom;
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
    for error in errors {
        match error {
            FluentError::ResolverError(ref err) => match err {
                ResolverError::Reference(ref s) => {
                    let error = format!("ReferenceError: {}", s);
                    ret_errors.push((&error).into());
                }
                ResolverError::MissingDefault => {
                    let error = "RangeError: No default";
                    ret_errors.push(error.into());
                }
                ResolverError::Cyclic => {
                    let error = "RangeError: Cyclic reference";
                    ret_errors.push(error.into());
                }
                ResolverError::TooManyPlaceables => {
                    let error = "RangeError: Too many placeables";
                    ret_errors.push(error.into());
                }
            },
            _ => panic!("Unknown error!"),
        }
    }
    true
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_add_resource(
    bundle: &mut FluentBundleRc,
    r: &FluentResource,
    allow_overrides: bool,
) {
    // we don't own the resource
    let r = mem::ManuallyDrop::new(Rc::from_raw(r));

    if allow_overrides {
        bundle.add_resource_overriding(Rc::clone(&r));
    } else if bundle.add_resource(Rc::clone(&r)).is_err() {
        eprintln!("Error while adding a resource");
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
