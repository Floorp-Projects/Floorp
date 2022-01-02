/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use fluent_langneg::negotiate::NegotiationStrategy as LangNegNegotiationStrategy;
use fluent_langneg::negotiate_languages;
use nsstring::nsACString;
use nsstring::nsCString;
use thin_vec::ThinVec;
use unic_langid::{LanguageIdentifier, LanguageIdentifierError};
use unic_langid_ffi::new_langid_for_mozilla;

/// We want to return the exact strings that were passed to us out of the
/// available and default pool. Since for the negotiation we canonicalize them
/// in `LanguageIdentifier`, this struct will preserve the original, non-canonicalized
/// string, and then use it to populate return array.
#[derive(Debug, PartialEq)]
struct LangIdString<'l> {
    pub source: &'l nsCString,
    pub langid: LanguageIdentifier,
}

impl<'l> LangIdString<'l> {
    pub fn try_new(s: &'l nsCString) -> Result<Self, LanguageIdentifierError> {
        new_langid_for_mozilla(s).map(|l| LangIdString {
            source: s,
            langid: l,
        })
    }
}

impl<'l> AsRef<LanguageIdentifier> for LangIdString<'l> {
    fn as_ref(&self) -> &LanguageIdentifier {
        &self.langid
    }
}

#[repr(C)]
pub enum NegotiationStrategy {
    Filtering,
    Matching,
    Lookup,
}

fn get_strategy(input: NegotiationStrategy) -> LangNegNegotiationStrategy {
    match input {
        NegotiationStrategy::Filtering => LangNegNegotiationStrategy::Filtering,
        NegotiationStrategy::Matching => LangNegNegotiationStrategy::Matching,
        NegotiationStrategy::Lookup => LangNegNegotiationStrategy::Lookup,
    }
}

#[no_mangle]
pub extern "C" fn fluent_langneg_negotiate_languages(
    requested: &ThinVec<nsCString>,
    available: &ThinVec<nsCString>,
    default: &nsACString,
    strategy: NegotiationStrategy,
    result: &mut ThinVec<nsCString>,
) {
    let requested = requested
        .iter()
        .filter_map(|s| new_langid_for_mozilla(s).ok())
        .collect::<Vec<_>>();

    let available = available
        .iter()
        .filter_map(|s| LangIdString::try_new(s).ok())
        .collect::<Vec<_>>();

    let d: nsCString = default.into();
    let default = LangIdString::try_new(&d).ok();

    let strategy = get_strategy(strategy);

    for l in negotiate_languages(&requested, &available, default.as_ref(), strategy) {
        result.push(l.source.clone());
    }
}
