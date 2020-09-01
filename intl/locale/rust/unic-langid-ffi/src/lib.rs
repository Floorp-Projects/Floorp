/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use nsstring::nsACString;
use nsstring::nsCString;
use thin_vec::ThinVec;
pub use unic_langid::{CharacterDirection, LanguageIdentifier, LanguageIdentifierError};

pub fn new_langid_for_mozilla(
    name: &nsACString,
) -> Result<LanguageIdentifier, LanguageIdentifierError> {
    if name.eq_ignore_ascii_case(b"ja-jp-mac") {
        "ja-JP-macos".parse()
    } else {
        // Cut out any `.FOO` like `en-US.POSIX`.
        let mut name: &[u8] = name.as_ref();
        if let Some(ptr) = name.iter().position(|b| b == &b'.') {
            name = &name[..ptr];
        }
        LanguageIdentifier::from_bytes(name)
    }
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_canonicalize(name: &mut nsACString) -> bool {
    let langid = new_langid_for_mozilla(name);

    let result = langid.is_ok();

    name.assign(&langid.unwrap_or_default().to_string());

    result
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_new(
    name: &nsACString,
    ret_val: &mut bool,
) -> *mut LanguageIdentifier {
    let langid = new_langid_for_mozilla(name);

    *ret_val = langid.is_ok();
    Box::into_raw(Box::new(langid.unwrap_or_default()))
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_destroy(langid: *mut LanguageIdentifier) {
    let _ = Box::from_raw(langid);
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_as_string(
    langid: &mut LanguageIdentifier,
    ret_val: &mut nsACString,
) {
    ret_val.assign(&langid.to_string());
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_get_language(
    langid: &LanguageIdentifier,
    len: *mut u32,
) -> *const u8 {
    let lang = langid.language();
    *len = lang.len() as u32;
    lang.as_bytes().as_ptr()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_set_language(
    langid: &mut LanguageIdentifier,
    string: &nsACString,
) -> bool {
    langid.set_language(string).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_clear_language(langid: &mut LanguageIdentifier) {
    langid.clear_language()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_get_script(
    langid: &LanguageIdentifier,
    len: *mut u32,
) -> *const u8 {
    let script = langid.script().unwrap_or("");
    *len = script.len() as u32;
    script.as_bytes().as_ptr()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_set_script(
    langid: &mut LanguageIdentifier,
    string: &nsACString,
) -> bool {
    langid.set_script(string).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_clear_script(langid: &mut LanguageIdentifier) {
    langid.clear_script()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_get_region(
    langid: &LanguageIdentifier,
    len: *mut u32,
) -> *const u8 {
    let region = langid.region().unwrap_or("");
    *len = region.len() as u32;
    region.as_bytes().as_ptr()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_set_region(
    langid: &mut LanguageIdentifier,
    string: &nsACString,
) -> bool {
    langid.set_region(string).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_clear_region(langid: &mut LanguageIdentifier) {
    langid.clear_region()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_get_variants(
    langid: &LanguageIdentifier,
    variants: &mut ThinVec<nsCString>,
) {
    for v in langid.variants() {
        variants.push(v.into());
    }
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_set_variants(
    langid: &mut LanguageIdentifier,
    variants: &ThinVec<nsCString>,
) -> bool {
    langid.set_variants(variants).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_clear_variants(langid: &mut LanguageIdentifier) {
    langid.clear_variants()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_matches(
    langid: &LanguageIdentifier,
    other: &LanguageIdentifier,
    self_as_range: bool,
    other_as_range: bool,
) -> bool {
    langid.matches(other, self_as_range, other_as_range)
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_maximize(langid: &mut LanguageIdentifier) -> bool {
    langid.maximize()
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_is_rtl(name: &nsACString) -> bool {
    match new_langid_for_mozilla(name) {
        Ok(langid) => langid.character_direction() == CharacterDirection::RTL,
        Err(_) => false,
    }
}
