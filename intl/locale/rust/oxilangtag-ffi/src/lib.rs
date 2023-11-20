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

/// Matches an HTML language attribute against a CSS :lang() selector using the
/// "extended filtering" algorithm.
/// The attribute is a BCP47 language tag that was successfully parsed by oxilangtag;
/// the selector is a string that is treated as a language range per RFC 4647.
#[no_mangle]
pub extern "C" fn lang_tag_matches(attribute: *const LangTag, selector: &nsACString) -> bool {
    // This should only be called with a pointer that we got from lang_tag_new().
    let lang = unsafe { *(attribute as *const LanguageTag<&str>) };

    // Our callers guarantee that the selector string is valid UTF-8.
    let range_str = unsafe { selector.as_str_unchecked() };

    if lang.is_empty() || range_str.is_empty() {
        return false;
    }

    // RFC 4647 Extended Filtering:
    // https://datatracker.ietf.org/doc/html/rfc4647#section-3.3.2

    // 1.  Split both the extended language range and the language tag being
    // compared into a list of subtags by dividing on the hyphen (%x2D)
    // character.  Two subtags match if either they are the same when
    // compared case-insensitively or the language range's subtag is the
    // wildcard '*'.

    let mut range_subtags = range_str.split('-');
    let mut lang_subtags = lang.as_str().split('-');

    // 2.  Begin with the first subtag in each list.  If the first subtag in
    // the range does not match the first subtag in the tag, the overall
    // match fails.  Otherwise, move to the next subtag in both the
    // range and the tag.

    let mut range_subtag = range_subtags.next();
    let mut lang_subtag = lang_subtags.next();
    // Cannot be None, because we checked that both args were non-empty.
    assert!(range_subtag.is_some() && lang_subtag.is_some());
    if !(range_subtag.unwrap() == "*"
        || range_subtag
            .unwrap()
            .eq_ignore_ascii_case(lang_subtag.unwrap()))
    {
        return false;
    }

    range_subtag = range_subtags.next();
    lang_subtag = lang_subtags.next();

    // 3.  While there are more subtags left in the language range's list:
    loop {
        // 4.  When the language range's list has no more subtags, the match
        // succeeds.
        let Some(range_subtag_str) = range_subtag else {
            return true;
        };

        // A.  If the subtag currently being examined in the range is the
        //     wildcard ('*'), move to the next subtag in the range and
        //     continue with the loop.
        if range_subtag_str == "*" {
            range_subtag = range_subtags.next();
            continue;
        }

        // B.  Else, if there are no more subtags in the language tag's
        //     list, the match fails.
        let Some(lang_subtag_str) = lang_subtag else {
            return false;
        };

        // C.  Else, if the current subtag in the range's list matches the
        //     current subtag in the language tag's list, move to the next
        //     subtag in both lists and continue with the loop.
        if range_subtag_str.eq_ignore_ascii_case(lang_subtag_str) {
            range_subtag = range_subtags.next();
            lang_subtag = lang_subtags.next();
            continue;
        }

        // D.  Else, if the language tag's subtag is a "singleton" (a single
        //     letter or digit, which includes the private-use subtag 'x')
        //     the match fails.
        if lang_subtag_str.len() == 1 {
            return false;
        }

        // E.  Else, move to the next subtag in the language tag's list and
        //     continue with the loop.
        lang_subtag = lang_subtags.next();
    }
}
