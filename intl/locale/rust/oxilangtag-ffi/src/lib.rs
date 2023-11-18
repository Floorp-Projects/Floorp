/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use nsstring::nsACString;
use oxilangtag::LanguageTag;

pub struct LangTag; // Opaque type for ffi interface.

/// Parse a string as a BCP47 language tag. Returns a `LangTag` object if the string is
/// successfully parsed; this must be freed with `lang_tag_destroy`.
///
/// The string `tag` must outlive the `LangTag`.
///
/// Returns null if `tag` is not a well-formed BCP47 tag (including if it is not
/// valid UTF-8).
#[no_mangle]
pub extern "C" fn lang_tag_new(tag: &nsACString) -> *mut LangTag {
    if let Ok(tag_str) = core::str::from_utf8(tag.as_ref()) {
        if let Ok(language_tag) = LanguageTag::parse(tag_str) {
            return Box::into_raw(Box::new(language_tag)) as *mut LangTag;
        }
    }
    std::ptr::null_mut()
}

/// Free a `LangTag` instance.
#[no_mangle]
pub extern "C" fn lang_tag_destroy(lang: *mut LangTag) {
    if lang.is_null() {
        return;
    }
    let _ = unsafe { Box::from_raw(lang as *mut LanguageTag<&str>) };
}

/// Matches an HTML language attribute against a CSS :lang() selector.
/// The attribute is a normal language tag; the selector is a language range,
/// with `und` representing a "wildcard" primary language.
/// (Based on LanguageTag::matches from the rust-language-tags crate,
/// adapted to this specific use case.)
#[no_mangle]
pub extern "C" fn lang_tag_matches(attribute: *const LangTag, selector: *const LangTag) -> bool {
    let lang = unsafe { *(attribute as *const LanguageTag<&str>) };
    let range = unsafe { *(selector as *const LanguageTag<&str>) };

    fn matches_option(a: Option<&str>, b: Option<&str>) -> bool {
        match (a, b) {
            (Some(a), Some(b)) => a.eq_ignore_ascii_case(b),
            (_, None) => true,
            (None, _) => false,
        }
    }

    fn matches_iter<'a>(
        a: impl Iterator<Item = &'a str>,
        b: impl Iterator<Item = &'a str>,
    ) -> bool {
        a.zip(b).all(|(x, y)| x.eq_ignore_ascii_case(y))
    }

    if !(lang
        .primary_language()
        .eq_ignore_ascii_case(range.primary_language())
        || range.primary_language().eq_ignore_ascii_case("und"))
    {
        return false;
    }

    matches_option(lang.script(), range.script())
        && matches_option(lang.region(), range.region())
        && matches_iter(lang.variant_subtags(), range.variant_subtags())
        && matches_iter(
            lang.extended_language_subtags(),
            range.extended_language_subtags(),
        )
        && matches_option(lang.private_use(), range.private_use())
}
