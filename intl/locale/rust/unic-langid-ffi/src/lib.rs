/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use nsstring::{nsACString, nsCStr, nsCString};
use thin_vec::ThinVec;
pub use unic_langid::{subtags, CharacterDirection, LanguageIdentifier, LanguageIdentifierError};

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
pub extern "C" fn unic_langid_canonicalize(name: &mut nsACString) -> bool {
    let langid = new_langid_for_mozilla(name);

    let result = langid.is_ok();

    name.assign(&langid.unwrap_or_default().to_string());

    result
}

#[no_mangle]
pub extern "C" fn unic_langid_new(
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
pub extern "C" fn unic_langid_as_string(langid: &mut LanguageIdentifier, ret_val: &mut nsACString) {
    ret_val.assign(&langid.to_string());
}

#[no_mangle]
pub extern "C" fn unic_langid_get_language<'a>(
    langid: &'a LanguageIdentifier,
    out: &mut nsCStr<'a>,
) {
    *out = nsCStr::from(langid.language.as_str());
}

#[no_mangle]
pub extern "C" fn unic_langid_set_language(
    langid: &mut LanguageIdentifier,
    string: &nsACString,
) -> bool {
    subtags::Language::from_bytes(string)
        .map(|lang| langid.language = lang)
        .is_ok()
}

#[no_mangle]
pub extern "C" fn unic_langid_clear_language(langid: &mut LanguageIdentifier) {
    langid.language.clear()
}

#[no_mangle]
pub extern "C" fn unic_langid_get_script<'a>(langid: &'a LanguageIdentifier, out: &mut nsCStr<'a>) {
    *out = nsCStr::from(langid.script.as_ref().map_or("", |s| s.as_str()));
}

#[no_mangle]
pub extern "C" fn unic_langid_set_script(
    langid: &mut LanguageIdentifier,
    string: &nsACString,
) -> bool {
    subtags::Script::from_bytes(string)
        .map(|script| langid.script = Some(script))
        .is_ok()
}

#[no_mangle]
pub extern "C" fn unic_langid_clear_script(langid: &mut LanguageIdentifier) {
    langid.script = None;
}

#[no_mangle]
pub extern "C" fn unic_langid_get_region<'a>(langid: &'a LanguageIdentifier, out: &mut nsCStr<'a>) {
    *out = nsCStr::from(langid.region.as_ref().map_or("", |s| s.as_str()));
}

#[no_mangle]
pub extern "C" fn unic_langid_set_region(
    langid: &mut LanguageIdentifier,
    string: &nsACString,
) -> bool {
    subtags::Region::from_bytes(string)
        .map(|region| langid.region = Some(region))
        .is_ok()
}

#[no_mangle]
pub extern "C" fn unic_langid_clear_region(langid: &mut LanguageIdentifier) {
    langid.region = None;
}

#[no_mangle]
pub extern "C" fn unic_langid_get_variants(
    langid: &LanguageIdentifier,
    variants: &mut ThinVec<nsCString>,
) {
    for v in langid.variants() {
        variants.push(v.as_str().into());
    }
}

#[no_mangle]
pub extern "C" fn unic_langid_set_variants(
    langid: &mut LanguageIdentifier,
    variants: &ThinVec<nsCString>,
) -> bool {
    variants
        .iter()
        .map(|v| subtags::Variant::from_bytes(v))
        .collect::<Result<Vec<_>, _>>()
        .map(|variants| langid.set_variants(&variants))
        .is_ok()
}

#[no_mangle]
pub extern "C" fn unic_langid_clear_variants(langid: &mut LanguageIdentifier) {
    langid.clear_variants()
}

#[no_mangle]
pub extern "C" fn unic_langid_matches(
    langid: &LanguageIdentifier,
    other: &LanguageIdentifier,
    self_as_range: bool,
    other_as_range: bool,
) -> bool {
    langid.matches(other, self_as_range, other_as_range)
}

#[no_mangle]
pub extern "C" fn unic_langid_maximize(langid: &mut LanguageIdentifier) -> bool {
    langid.maximize()
}

#[no_mangle]
pub extern "C" fn unic_langid_is_rtl(name: &nsACString) -> bool {
    match new_langid_for_mozilla(name) {
        Ok(langid) => langid.character_direction() == CharacterDirection::RTL,
        Err(_) => false,
    }
}
