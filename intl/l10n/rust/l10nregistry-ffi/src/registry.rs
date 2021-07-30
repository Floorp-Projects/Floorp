/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use fluent_ffi::{adapt_bundle_for_gecko, FluentBundleRc};
use nsstring::{nsACString, nsCString};
use std::mem;
use std::rc::Rc;
use thin_vec::ThinVec;

use crate::{env::GeckoEnvironment, fetcher::GeckoFileFetcher};
use fluent_fallback::generator::BundleGenerator;
use futures_channel::mpsc::{unbounded, UnboundedSender};
pub use l10nregistry::{
    errors::L10nRegistrySetupError,
    registry::{BundleAdapter, GenerateBundles, GenerateBundlesSync, L10nRegistry},
    source::FileSource,
};
use unic_langid::LanguageIdentifier;
use xpcom::RefPtr;

#[derive(Clone)]
pub struct GeckoBundleAdapter {
    use_isolating: bool,
}

impl Default for GeckoBundleAdapter {
    fn default() -> Self {
        Self {
            use_isolating: true,
        }
    }
}

impl BundleAdapter for GeckoBundleAdapter {
    fn adapt_bundle(&self, bundle: &mut l10nregistry::fluent::FluentBundle) {
        bundle.set_use_isolating(self.use_isolating);
        adapt_bundle_for_gecko(bundle, None);
    }
}

thread_local!(static L10N_REGISTRY: Rc<GeckoL10nRegistry> = {
    let env = GeckoEnvironment;
    let mut reg = L10nRegistry::with_provider(env);

    let packaged_locales = vec!["en-US".parse().unwrap()];

    let toolkit_fs = FileSource::new(
        "0-toolkit".to_owned(),
        packaged_locales.clone(),
        "resource://gre/localization/{locale}/".to_owned(),
        Default::default(),
        GeckoFileFetcher,
    );
    let browser_fs = FileSource::new(
        "5-browser".to_owned(),
        packaged_locales,
        "resource://app/localization/{locale}/".to_owned(),
        Default::default(),
        GeckoFileFetcher,
    );
    let _ = reg.set_adapt_bundle(GeckoBundleAdapter::default()).report_error();

    let _ = reg.register_sources(vec![toolkit_fs, browser_fs]).report_error();

    Rc::new(reg)
});

pub type GeckoL10nRegistry = L10nRegistry<GeckoEnvironment, GeckoBundleAdapter>;
pub type GeckoFluentBundleIterator = GenerateBundlesSync<GeckoEnvironment, GeckoBundleAdapter>;

trait GeckoReportError<V, E> {
    fn report_error(self) -> Result<V, E>;
}

impl<V> GeckoReportError<V, L10nRegistrySetupError> for Result<V, L10nRegistrySetupError> {
    fn report_error(self) -> Self {
        if let Err(ref err) = self {
            GeckoEnvironment::report_l10nregistry_setup_error(err);
        }
        self
    }
}

pub fn get_l10n_registry() -> Rc<GeckoL10nRegistry> {
    L10N_REGISTRY.with(|reg| reg.clone())
}

#[repr(C)]
pub enum L10nRegistryStatus {
    None,
    EmptyName,
    InvalidLocaleCode,
}

#[no_mangle]
pub extern "C" fn l10nregistry_new() -> *const GeckoL10nRegistry {
    let env = GeckoEnvironment;
    let mut reg = L10nRegistry::with_provider(env);
    let _ = reg
        .set_adapt_bundle(GeckoBundleAdapter::default())
        .report_error();
    Rc::into_raw(Rc::new(reg))
}

#[no_mangle]
pub extern "C" fn l10nregistry_instance_get() -> *const GeckoL10nRegistry {
    let reg = get_l10n_registry();
    Rc::into_raw(reg)
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_addref(reg: &GeckoL10nRegistry) {
    let raw = Rc::from_raw(reg);
    mem::forget(Rc::clone(&raw));
    mem::forget(raw);
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_release(reg: &GeckoL10nRegistry) {
    let _ = Rc::from_raw(reg);
}

#[no_mangle]
pub extern "C" fn l10nregistry_get_available_locales(
    reg: &GeckoL10nRegistry,
    result: &mut ThinVec<nsCString>,
) {
    if let Ok(locales) = reg.get_available_locales().report_error() {
        result.extend(locales.into_iter().map(|locale| locale.to_string().into()));
    }
}

#[no_mangle]
pub extern "C" fn l10nregistry_register_sources(
    reg: &GeckoL10nRegistry,
    sources: &ThinVec<&FileSource>,
) {
    let _ = reg
        .register_sources(sources.iter().map(|&s| s.clone()).collect())
        .report_error();
}

#[no_mangle]
pub extern "C" fn l10nregistry_update_sources(
    reg: &GeckoL10nRegistry,
    sources: &mut ThinVec<&FileSource>,
) {
    let _ = reg
        .update_sources(sources.iter().map(|&s| s.clone()).collect())
        .report_error();
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_remove_sources(
    reg: &GeckoL10nRegistry,
    sources_elements: *const nsCString,
    sources_length: usize,
) {
    if sources_elements.is_null() {
        return;
    }

    let sources = std::slice::from_raw_parts(sources_elements, sources_length);
    let _ = reg.remove_sources(sources.to_vec()).report_error();
}

#[no_mangle]
pub extern "C" fn l10nregistry_has_source(
    reg: &GeckoL10nRegistry,
    name: &nsACString,
    status: &mut L10nRegistryStatus,
) -> bool {
    if name.is_empty() {
        *status = L10nRegistryStatus::EmptyName;
        return false;
    }
    *status = L10nRegistryStatus::None;
    reg.has_source(&name.to_utf8())
        .report_error()
        .unwrap_or(false)
}

#[no_mangle]
pub extern "C" fn l10nregistry_get_source(
    reg: &GeckoL10nRegistry,
    name: &nsACString,
    status: &mut L10nRegistryStatus,
) -> *mut FileSource {
    if name.is_empty() {
        *status = L10nRegistryStatus::EmptyName;
        return std::ptr::null_mut();
    }

    *status = L10nRegistryStatus::None;

    if let Ok(Some(source)) = reg.get_source(&name.to_utf8()).report_error() {
        Box::into_raw(Box::new(source))
    } else {
        std::ptr::null_mut()
    }
}

#[no_mangle]
pub extern "C" fn l10nregistry_clear_sources(reg: &GeckoL10nRegistry) {
    let _ = reg.clear_sources().report_error();
}

#[no_mangle]
pub extern "C" fn l10nregistry_get_source_names(
    reg: &GeckoL10nRegistry,
    result: &mut ThinVec<nsCString>,
) {
    if let Ok(names) = reg.get_source_names().report_error() {
        result.extend(names.into_iter().map(|name| nsCString::from(name)));
    }
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_generate_bundles_sync(
    reg: &GeckoL10nRegistry,
    locales_elements: *const nsCString,
    locales_length: usize,
    res_ids_elements: *const nsCString,
    res_ids_length: usize,
    status: &mut L10nRegistryStatus,
) -> *mut GeckoFluentBundleIterator {
    let locales = std::slice::from_raw_parts(locales_elements, locales_length);
    let res_ids = std::slice::from_raw_parts(res_ids_elements, res_ids_length);

    let locales: Result<Vec<LanguageIdentifier>, _> =
        locales.into_iter().map(|s| s.to_utf8().parse()).collect();

    match locales {
        Ok(locales) => {
            *status = L10nRegistryStatus::None;
            let res_ids = res_ids.into_iter().map(|s| s.to_string()).collect();
            let iter = reg.bundles_iter(locales.into_iter(), res_ids);
            Box::into_raw(Box::new(iter))
        }
        Err(_) => {
            *status = L10nRegistryStatus::InvalidLocaleCode;
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_iterator_destroy(iter: *mut GeckoFluentBundleIterator) {
    let _ = Box::from_raw(iter);
}

#[no_mangle]
pub extern "C" fn fluent_bundle_iterator_next(
    iter: &mut GeckoFluentBundleIterator,
) -> *mut FluentBundleRc {
    if let Some(Ok(result)) = iter.next() {
        Box::into_raw(Box::new(result))
    } else {
        std::ptr::null_mut()
    }
}

pub struct NextRequest {
    promise: RefPtr<xpcom::Promise>,
    // Ownership is transferred here.
    callback: unsafe extern "C" fn(&xpcom::Promise, *mut FluentBundleRc),
}

pub struct GeckoFluentBundleAsyncIteratorWrapper(UnboundedSender<NextRequest>);

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_generate_bundles(
    reg: &GeckoL10nRegistry,
    locales_elements: *const nsCString,
    locales_length: usize,
    res_ids_elements: *const nsCString,
    res_ids_length: usize,
    status: &mut L10nRegistryStatus,
) -> *mut GeckoFluentBundleAsyncIteratorWrapper {
    let locales = std::slice::from_raw_parts(locales_elements, locales_length);
    let res_ids = std::slice::from_raw_parts(res_ids_elements, res_ids_length);
    let locales: Result<Vec<LanguageIdentifier>, _> =
        locales.into_iter().map(|s| s.to_utf8().parse()).collect();

    match locales {
        Ok(locales) => {
            *status = L10nRegistryStatus::None;
            let res_ids = res_ids.into_iter().map(|s| s.to_string()).collect();
            let mut iter = reg.bundles_stream(locales.into_iter(), res_ids);

            // Immediately spawn the task which will handle the async calls, and use an `UnboundedSender`
            // to send callbacks for specific `next()` calls to it.
            let (sender, mut receiver) = unbounded::<NextRequest>();
            moz_task::spawn_current_thread(async move {
                use futures::StreamExt;
                while let Some(req) = receiver.next().await {
                    let result = match iter.next().await {
                        Some(Ok(result)) => Box::into_raw(Box::new(result)),
                        _ => std::ptr::null_mut(),
                    };
                    (req.callback)(&req.promise, result);
                }
            })
            .expect("Failed to spawn a task");
            let iter = GeckoFluentBundleAsyncIteratorWrapper(sender);
            Box::into_raw(Box::new(iter))
        }
        Err(_) => {
            *status = L10nRegistryStatus::InvalidLocaleCode;
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn fluent_bundle_async_iterator_destroy(
    iter: *mut GeckoFluentBundleAsyncIteratorWrapper,
) {
    let _ = Box::from_raw(iter);
}

#[no_mangle]
pub extern "C" fn fluent_bundle_async_iterator_next(
    iter: &GeckoFluentBundleAsyncIteratorWrapper,
    promise: &xpcom::Promise,
    callback: extern "C" fn(&xpcom::Promise, *mut FluentBundleRc),
) {
    if iter
        .0
        .unbounded_send(NextRequest {
            promise: RefPtr::new(promise),
            callback,
        })
        .is_err()
    {
        callback(promise, std::ptr::null_mut());
    }
}
