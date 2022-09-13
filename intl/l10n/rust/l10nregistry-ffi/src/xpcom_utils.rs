/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use cstr::cstr;
use nsstring::{nsACString, nsCString};
use std::marker::PhantomData;
use thin_vec::ThinVec;
use xpcom::{
    get_service, getter_addrefs,
    interfaces::{
        mozILocaleService, nsICategoryEntry, nsICategoryManager, nsISimpleEnumerator, nsIXULRuntime,
    },
    RefPtr, XpCom,
};

pub struct IterSimpleEnumerator<T> {
    enumerator: RefPtr<nsISimpleEnumerator>,
    phantom: PhantomData<T>,
}

impl<T: XpCom> IterSimpleEnumerator<T> {
    /// Convert a `nsISimpleEnumerator` into a rust `Iterator` type.
    pub fn new(enumerator: RefPtr<nsISimpleEnumerator>) -> Self {
        IterSimpleEnumerator {
            enumerator,
            phantom: PhantomData,
        }
    }
}

impl<T: XpCom + 'static> Iterator for IterSimpleEnumerator<T> {
    type Item = RefPtr<T>;

    fn next(&mut self) -> Option<Self::Item> {
        let mut more = false;
        unsafe {
            self.enumerator
                .HasMoreElements(&mut more)
                .to_result()
                .ok()?
        }
        if !more {
            return None;
        }

        let element = getter_addrefs(|p| unsafe { self.enumerator.GetNext(p) }).ok()?;
        element.query_interface::<T>()
    }
}

fn process_type() -> u32 {
    if let Ok(appinfo) = xpcom::components::XULRuntime::service::<nsIXULRuntime>() {
        let mut process_type = nsIXULRuntime::PROCESS_TYPE_DEFAULT;
        if unsafe { appinfo.GetProcessType(&mut process_type).succeeded() } {
            return process_type;
        }
    }
    nsIXULRuntime::PROCESS_TYPE_DEFAULT
}

pub fn is_parent_process() -> bool {
    process_type() == nsIXULRuntime::PROCESS_TYPE_DEFAULT
}

pub fn get_packaged_locales() -> Option<ThinVec<nsCString>> {
    let locale_service =
        get_service::<mozILocaleService>(cstr!("@mozilla.org/intl/localeservice;1"))?;
    let mut locales = ThinVec::new();
    unsafe {
        locale_service
            .GetPackagedLocales(&mut locales)
            .to_result()
            .ok()?;
    }
    Some(locales)
}

pub fn get_app_locales() -> Option<ThinVec<nsCString>> {
    let locale_service =
        get_service::<mozILocaleService>(cstr!("@mozilla.org/intl/localeservice;1"))?;
    let mut locales = ThinVec::new();
    unsafe {
        locale_service
            .GetAppLocalesAsBCP47(&mut locales)
            .to_result()
            .ok()?;
    }
    Some(locales)
}

pub fn set_available_locales(locales: &ThinVec<nsCString>) {
    let locale_service =
        get_service::<mozILocaleService>(cstr!("@mozilla.org/intl/localeservice;1"))
            .expect("Failed to get a service.");
    unsafe {
        locale_service
            .SetAvailableLocales(locales)
            .to_result()
            .expect("Failed to set locales.");
    }
}

pub struct CategoryEntry {
    pub entry: nsCString,
    pub value: nsCString,
}

pub fn get_category_entries(category: &nsACString) -> Option<Vec<CategoryEntry>> {
    let category_manager =
        get_service::<nsICategoryManager>(cstr!("@mozilla.org/categorymanager;1"))?;

    let enumerator =
        getter_addrefs(|p| unsafe { category_manager.EnumerateCategory(category, p) }).ok()?;

    Some(
        IterSimpleEnumerator::<nsICategoryEntry>::new(enumerator)
            .map(|ientry| {
                let mut entry = nsCString::new();
                let mut value = nsCString::new();
                unsafe {
                    let _ = ientry.GetEntry(&mut *entry);
                    let _ = ientry.GetValue(&mut *value);
                }
                CategoryEntry { entry, value }
            })
            .collect(),
    )
}
