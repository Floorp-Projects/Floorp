/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use fluent_ffi::{adapt_bundle_for_gecko, FluentBundleRc};
use nsstring::{nsACString, nsCString};
use std::mem;
use std::rc::Rc;
use thin_vec::ThinVec;

use crate::{env::GeckoEnvironment, fetcher::GeckoFileFetcher, xpcom_utils::is_parent_process};
use fluent_fallback::{generator::BundleGenerator, types::ResourceType};
use futures_channel::mpsc::{unbounded, UnboundedSender};
pub use l10nregistry::{
    errors::L10nRegistrySetupError,
    registry::{BundleAdapter, GenerateBundles, GenerateBundlesSync, L10nRegistry},
    source::{FileSource, ResourceId, ToResourceId},
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
    let sources = if is_parent_process() {
        let packaged_locales = get_packaged_locales();
        let entries = get_l10n_registry_category_entries();

        Some(entries
             .into_iter()
             .map(|entry| {
                 FileSource::new(
                     entry.entry.to_string(),
                     Some("app".to_string()),
                     packaged_locales.clone(),
                     entry.value.to_string(),
                     Default::default(),
                     GeckoFileFetcher,
                 )
             })
             .collect())

    } else {
        None
    };

    create_l10n_registry(sources)
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

#[derive(Debug)]
#[repr(C)]
pub struct L10nFileSourceDescriptor {
    name: nsCString,
    metasource: nsCString,
    locales: ThinVec<nsCString>,
    pre_path: nsCString,
    index: ThinVec<nsCString>,
}

fn get_l10n_registry_category_entries() -> Vec<crate::xpcom_utils::CategoryEntry> {
    crate::xpcom_utils::get_category_entries(&nsCString::from("l10n-registry")).unwrap_or_default()
}

fn get_packaged_locales() -> Vec<LanguageIdentifier> {
    crate::xpcom_utils::get_packaged_locales()
        .map(|locales| {
            locales
                .into_iter()
                .map(|s| s.to_utf8().parse().expect("Failed to parse locale."))
                .collect()
        })
        .unwrap_or_default()
}

fn create_l10n_registry(sources: Option<Vec<FileSource>>) -> Rc<GeckoL10nRegistry> {
    let env = GeckoEnvironment::new(None);
    let mut reg = L10nRegistry::with_provider(env);

    reg.set_bundle_adapter(GeckoBundleAdapter::default())
        .expect("Failed to set bundle adaptation closure.");

    if let Some(sources) = sources {
        reg.register_sources(sources)
            .expect("Failed to register sources.");
    }
    Rc::new(reg)
}

pub fn set_l10n_registry(new_sources: &ThinVec<L10nFileSourceDescriptor>) {
    L10N_REGISTRY.with(|reg| {
        let new_source_names: Vec<_> = new_sources
            .iter()
            .map(|d| d.name.to_utf8().to_string())
            .collect();
        let old_sources = reg.get_source_names().unwrap();

        let mut sources_to_be_removed = vec![];
        for name in &old_sources {
            if !new_source_names.contains(&name) {
                sources_to_be_removed.push(name);
            }
        }
        reg.remove_sources(sources_to_be_removed).unwrap();

        let mut add_sources = vec![];
        for desc in new_sources {
            if !old_sources.contains(&desc.name.to_string()) {
                add_sources.push(FileSource::new(
                    desc.name.to_string(),
                    Some(desc.metasource.to_string()),
                    desc.locales
                        .iter()
                        .map(|s| s.to_utf8().parse().unwrap())
                        .collect(),
                    desc.pre_path.to_string(),
                    Default::default(),
                    GeckoFileFetcher,
                ));
            }
        }
        reg.register_sources(add_sources).unwrap();
    });
}

pub fn get_l10n_registry() -> Rc<GeckoL10nRegistry> {
    L10N_REGISTRY.with(|reg| reg.clone())
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum GeckoResourceType {
    Optional,
    Required,
}

#[repr(C)]
pub struct GeckoResourceId {
    value: nsCString,
    resource_type: GeckoResourceType,
}

impl From<&GeckoResourceId> for ResourceId {
    fn from(resource_id: &GeckoResourceId) -> Self {
        resource_id
            .value
            .to_string()
            .to_resource_id(match resource_id.resource_type {
                GeckoResourceType::Optional => ResourceType::Optional,
                GeckoResourceType::Required => ResourceType::Required,
            })
    }
}

#[repr(C)]
pub enum L10nRegistryStatus {
    None,
    EmptyName,
    InvalidLocaleCode,
}

#[no_mangle]
pub extern "C" fn l10nregistry_new(use_isolating: bool) -> *const GeckoL10nRegistry {
    let env = GeckoEnvironment::new(None);
    let mut reg = L10nRegistry::with_provider(env);
    let _ = reg
        .set_bundle_adapter(GeckoBundleAdapter { use_isolating })
        .report_error();
    Rc::into_raw(Rc::new(reg))
}

#[no_mangle]
pub extern "C" fn l10nregistry_instance_get() -> *const GeckoL10nRegistry {
    let reg = get_l10n_registry();
    Rc::into_raw(reg)
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_get_parent_process_sources(
    sources: &mut ThinVec<L10nFileSourceDescriptor>,
) {
    debug_assert!(
        is_parent_process(),
        "This should be called only in parent process."
    );

    // If at the point when the first content process is being initialized, the parent
    // process `L10nRegistryService` has not been initialized yet, this will trigger it.
    //
    // This is architecturally imperfect, but acceptable for simplicity reasons because
    // `L10nRegistry` instance is cheap and mainly servers as a store of state.
    let reg = get_l10n_registry();
    for name in reg.get_source_names().unwrap() {
        let source = reg.file_source_by_name(&name).unwrap().unwrap();
        let descriptor = L10nFileSourceDescriptor {
            name: source.name.as_str().into(),
            metasource: source.metasource.as_str().into(),
            locales: source
                .locales()
                .iter()
                .map(|l| l.to_string().into())
                .collect(),
            pre_path: source.pre_path.as_str().into(),
            index: source
                .get_index()
                .map(|index| index.into_iter().map(|s| s.into()).collect())
                .unwrap_or_default(),
        };
        sources.push(descriptor);
    }
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_register_parent_process_sources(
    sources: &ThinVec<L10nFileSourceDescriptor>,
) {
    debug_assert!(
        !is_parent_process(),
        "This should be called only in content process."
    );
    set_l10n_registry(sources);
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_addref(reg: *const GeckoL10nRegistry) {
    let raw = Rc::from_raw(reg);
    mem::forget(Rc::clone(&raw));
    mem::forget(raw);
}

#[no_mangle]
pub unsafe extern "C" fn l10nregistry_release(reg: *const GeckoL10nRegistry) {
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

fn broadcast_settings_if_parent(reg: &GeckoL10nRegistry) {
    if !is_parent_process() {
        return;
    }

    L10N_REGISTRY.with(|reg_service| {
        if std::ptr::eq(Rc::as_ptr(reg_service), reg) {
            let locales = reg
                .get_available_locales()
                .unwrap()
                .iter()
                .map(|loc| loc.to_string().into())
                .collect();

            unsafe {
                crate::xpcom_utils::set_available_locales(&locales);
                L10nRegistrySendUpdateL10nFileSources();
            }
        }
    });
}

#[no_mangle]
pub extern "C" fn l10nregistry_register_sources(
    reg: &GeckoL10nRegistry,
    sources: &ThinVec<&FileSource>,
) {
    let _ = reg
        .register_sources(sources.iter().map(|&s| s.clone()).collect())
        .report_error();

    broadcast_settings_if_parent(reg);
}

#[no_mangle]
pub extern "C" fn l10nregistry_update_sources(
    reg: &GeckoL10nRegistry,
    sources: &mut ThinVec<&FileSource>,
) {
    let _ = reg
        .update_sources(sources.iter().map(|&s| s.clone()).collect())
        .report_error();
    broadcast_settings_if_parent(reg);
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
    broadcast_settings_if_parent(reg);
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

    if let Ok(Some(source)) = reg.file_source_by_name(&name.to_utf8()).report_error() {
        Box::into_raw(Box::new(source))
    } else {
        std::ptr::null_mut()
    }
}

#[no_mangle]
pub extern "C" fn l10nregistry_clear_sources(reg: &GeckoL10nRegistry) {
    let _ = reg.clear_sources().report_error();

    broadcast_settings_if_parent(reg);
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
    res_ids_elements: *const GeckoResourceId,
    res_ids_length: usize,
    status: &mut L10nRegistryStatus,
) -> *mut GeckoFluentBundleIterator {
    let locales = std::slice::from_raw_parts(locales_elements, locales_length);
    let res_ids = std::slice::from_raw_parts(res_ids_elements, res_ids_length)
        .into_iter()
        .map(ResourceId::from)
        .collect();
    let locales: Result<Vec<LanguageIdentifier>, _> =
        locales.into_iter().map(|s| s.to_utf8().parse()).collect();

    match locales {
        Ok(locales) => {
            *status = L10nRegistryStatus::None;
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
    res_ids_elements: *const GeckoResourceId,
    res_ids_length: usize,
    status: &mut L10nRegistryStatus,
) -> *mut GeckoFluentBundleAsyncIteratorWrapper {
    let locales = std::slice::from_raw_parts(locales_elements, locales_length);
    let res_ids = std::slice::from_raw_parts(res_ids_elements, res_ids_length)
        .into_iter()
        .map(ResourceId::from)
        .collect();
    let locales: Result<Vec<LanguageIdentifier>, _> =
        locales.into_iter().map(|s| s.to_utf8().parse()).collect();

    match locales {
        Ok(locales) => {
            *status = L10nRegistryStatus::None;
            let mut iter = reg.bundles_stream(locales.into_iter(), res_ids);

            // Immediately spawn the task which will handle the async calls, and use an `UnboundedSender`
            // to send callbacks for specific `next()` calls to it.
            let (sender, mut receiver) = unbounded::<NextRequest>();
            moz_task::spawn_local("l10nregistry_generate_bundles", async move {
                use futures::StreamExt;
                while let Some(req) = receiver.next().await {
                    let result = match iter.next().await {
                        Some(Ok(result)) => Box::into_raw(Box::new(result)),
                        _ => std::ptr::null_mut(),
                    };
                    (req.callback)(&req.promise, result);
                }
            })
            .detach();
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

extern "C" {
    pub fn L10nRegistrySendUpdateL10nFileSources();
}
