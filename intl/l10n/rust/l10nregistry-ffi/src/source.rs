/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use super::fetcher::{GeckoFileFetcher, MockFileFetcher};
use crate::env::GeckoEnvironment;

use fluent::FluentResource;
use l10nregistry::source::{FileSource, FileSourceOptions, ResourceStatus};

use nsstring::{nsACString, nsCString};
use thin_vec::ThinVec;
use unic_langid::LanguageIdentifier;

use std::{borrow::Cow, mem, rc::Rc};
use xpcom::RefPtr;

#[repr(C)]
pub enum L10nFileSourceStatus {
    None,
    EmptyName,
    EmptyPrePath,
    EmptyResId,
    InvalidLocaleCode,
}

// For historical reasons we maintain a locale in Firefox with a codename `ja-JP-mac`.
// This string is an invalid BCP-47 language tag, so we don't store it in Gecko, which uses
// valid BCP-47 tags only, but rather keep that quirk local to Gecko L10nRegistry file fetcher.
//
// Here, if we encounter `ja-JP-mac` (invalid BCP-47), we swap it for a valid equivalent: `ja-JP-macos`.
//
// See bug 1726586 for details and fetcher::get_locale_for_gecko.
fn get_locale_from_gecko<'s>(input: Cow<'s, str>) -> Cow<'s, str> {
    if input == "ja-JP-mac" {
        "ja-JP-macos".into()
    } else {
        input
    }
}

#[no_mangle]
pub extern "C" fn l10nfilesource_new(
    name: &nsACString,
    locales: &ThinVec<nsCString>,
    pre_path: &nsACString,
    allow_override: bool,
    status: &mut L10nFileSourceStatus,
) -> *const FileSource {
    if name.is_empty() {
        *status = L10nFileSourceStatus::EmptyName;
        return std::ptr::null();
    }
    if pre_path.is_empty() {
        *status = L10nFileSourceStatus::EmptyPrePath;
        return std::ptr::null();
    }

    let locales: Result<Vec<LanguageIdentifier>, _> = locales
        .iter()
        .map(|l| get_locale_from_gecko(l.to_utf8()).parse())
        .collect();

    let locales = match locales {
        Ok(locales) => locales,
        Err(..) => {
            *status = L10nFileSourceStatus::InvalidLocaleCode;
            return std::ptr::null();
        }
    };

    let mut source = FileSource::new(
        name.to_string(),
        locales,
        pre_path.to_string(),
        FileSourceOptions { allow_override },
        GeckoFileFetcher,
    );
    source.set_reporter(GeckoEnvironment::new(None));

    *status = L10nFileSourceStatus::None;
    Rc::into_raw(Rc::new(source))
}

#[no_mangle]
pub unsafe extern "C" fn l10nfilesource_new_with_index(
    name: &nsACString,
    locales: &ThinVec<nsCString>,
    pre_path: &nsACString,
    index_elements: *const nsCString,
    index_length: usize,
    allow_override: bool,
    status: &mut L10nFileSourceStatus,
) -> *const FileSource {
    if name.is_empty() {
        *status = L10nFileSourceStatus::EmptyName;
        return std::ptr::null();
    }
    if pre_path.is_empty() {
        *status = L10nFileSourceStatus::EmptyPrePath;
        return std::ptr::null();
    }

    let locales: Result<Vec<LanguageIdentifier>, _> = locales
        .iter()
        .map(|l| get_locale_from_gecko(l.to_utf8()).parse())
        .collect();

    let index = if index_length > 0 {
        assert!(!index_elements.is_null());
        std::slice::from_raw_parts(index_elements, index_length)
    } else {
        &[]
    }
    .into_iter()
    .map(|s| s.to_string())
    .collect();

    let locales = match locales {
        Ok(locales) => locales,
        Err(..) => {
            *status = L10nFileSourceStatus::InvalidLocaleCode;
            return std::ptr::null();
        }
    };

    let mut source = FileSource::new_with_index(
        name.to_string(),
        locales,
        pre_path.to_string(),
        FileSourceOptions { allow_override },
        GeckoFileFetcher,
        index,
    );
    source.set_reporter(GeckoEnvironment::new(None));

    *status = L10nFileSourceStatus::None;
    Rc::into_raw(Rc::new(source))
}

#[repr(C)]
pub struct L10nFileSourceMockFile {
    path: nsCString,
    source: nsCString,
}

#[no_mangle]
pub extern "C" fn l10nfilesource_new_mock(
    name: &nsACString,
    locales: &ThinVec<nsCString>,
    pre_path: &nsACString,
    fs: &ThinVec<L10nFileSourceMockFile>,
    status: &mut L10nFileSourceStatus,
) -> *const FileSource {
    if name.is_empty() {
        *status = L10nFileSourceStatus::EmptyName;
        return std::ptr::null();
    }
    if pre_path.is_empty() {
        *status = L10nFileSourceStatus::EmptyPrePath;
        return std::ptr::null();
    }

    let locales: Result<Vec<LanguageIdentifier>, _> = locales
        .iter()
        .map(|l| get_locale_from_gecko(l.to_utf8()).parse())
        .collect();

    let locales = match locales {
        Ok(locales) => locales,
        Err(..) => {
            *status = L10nFileSourceStatus::InvalidLocaleCode;
            return std::ptr::null();
        }
    };

    let fs = fs
        .iter()
        .map(|mock| (mock.path.to_string(), mock.source.to_string()))
        .collect();
    let fetcher = MockFileFetcher::new(fs);
    let mut source = FileSource::new(
        name.to_string(),
        locales,
        pre_path.to_string(),
        Default::default(),
        fetcher,
    );
    source.set_reporter(GeckoEnvironment::new(None));

    *status = L10nFileSourceStatus::None;
    Rc::into_raw(Rc::new(source))
}

#[no_mangle]
pub unsafe extern "C" fn l10nfilesource_addref(source: &FileSource) {
    let raw = Rc::from_raw(source);
    mem::forget(Rc::clone(&raw));
    mem::forget(raw);
}

#[no_mangle]
pub unsafe extern "C" fn l10nfilesource_release(source: *const FileSource) {
    let _ = Rc::from_raw(source);
}

#[no_mangle]
pub extern "C" fn l10nfilesource_get_name(source: &FileSource, ret_val: &mut nsACString) {
    ret_val.assign(&source.name);
}

#[no_mangle]
pub extern "C" fn l10nfilesource_get_locales(
    source: &FileSource,
    ret_val: &mut ThinVec<nsCString>,
) {
    for locale in source.locales() {
        ret_val.push(locale.to_string().into());
    }
}

#[no_mangle]
pub extern "C" fn l10nfilesource_get_prepath(source: &FileSource, ret_val: &mut nsACString) {
    ret_val.assign(&source.pre_path);
}

#[no_mangle]
pub extern "C" fn l10nfilesource_get_index(
    source: &FileSource,
    ret_val: &mut ThinVec<nsCString>,
) -> bool {
    if let Some(index) = source.get_index() {
        for entry in index {
            ret_val.push(entry.to_string().into());
        }
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn l10nfilesource_has_file(
    source: &FileSource,
    locale: &nsACString,
    path: &nsACString,
    status: &mut L10nFileSourceStatus,
    present: &mut bool,
) -> bool {
    if path.is_empty() {
        *status = L10nFileSourceStatus::EmptyResId;
        return false;
    }

    let locale = match locale.to_utf8().parse() {
        Ok(locale) => locale,
        Err(..) => {
            *status = L10nFileSourceStatus::InvalidLocaleCode;
            return false;
        }
    };

    *status = L10nFileSourceStatus::None;
    // To work around Option<bool> we return bool for the option,
    // and the `present` argument is the value of it.
    if let Some(val) = source.has_file(&locale, &path.to_utf8()) {
        *present = val;
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn l10nfilesource_fetch_file_sync(
    source: &FileSource,
    locale: &nsACString,
    path: &nsACString,
    status: &mut L10nFileSourceStatus,
) -> *const FluentResource {
    if path.is_empty() {
        *status = L10nFileSourceStatus::EmptyResId;
        return std::ptr::null();
    }

    let locale = match locale.to_utf8().parse() {
        Ok(locale) => locale,
        Err(..) => {
            *status = L10nFileSourceStatus::InvalidLocaleCode;
            return std::ptr::null();
        }
    };

    *status = L10nFileSourceStatus::None;
    //XXX: Bug 1723191 - if we encounter a request for sync load while async load is in progress
    //                   we will discard the async load and force the sync load instead.
    //                   There may be a better option but we haven't had time to explore it.
    if let Some(res) = source.fetch_file_sync(&locale, &path.to_utf8(), /* overload */ true) {
        Rc::into_raw(res)
    } else {
        std::ptr::null()
    }
}

#[no_mangle]
pub unsafe extern "C" fn l10nfilesource_fetch_file(
    source: &FileSource,
    locale: &nsACString,
    path: &nsACString,
    promise: &xpcom::Promise,
    callback: extern "C" fn(&xpcom::Promise, Option<&FluentResource>),
    status: &mut L10nFileSourceStatus,
) {
    if path.is_empty() {
        *status = L10nFileSourceStatus::EmptyResId;
        return;
    }

    let locale = match locale.to_utf8().parse() {
        Ok(locale) => locale,
        Err(..) => {
            *status = L10nFileSourceStatus::InvalidLocaleCode;
            return;
        }
    };

    *status = L10nFileSourceStatus::None;

    let path = path.to_utf8();

    match source.fetch_file(&locale, &path) {
        ResourceStatus::Missing => callback(promise, None),
        ResourceStatus::Loaded(res) => callback(promise, Some(&res)),
        res @ ResourceStatus::Loading(_) => {
            let strong_promise = RefPtr::new(promise);
            moz_task::spawn_current_thread(async move {
                callback(&strong_promise, res.await.as_ref().map(|r| &**r));
            })
            .expect("Failed to spawn future");
        }
    }
}
