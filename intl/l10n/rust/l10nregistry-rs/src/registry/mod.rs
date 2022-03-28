mod asynchronous;
mod synchronous;

use std::{collections::HashSet, rc::Rc, sync::Mutex, sync::MutexGuard};

use crate::errors::L10nRegistrySetupError;
use crate::source::{FileSource, ResourceId};

use crate::env::ErrorReporter;
use crate::fluent::FluentBundle;
use fluent_bundle::FluentResource;
use fluent_fallback::generator::BundleGenerator;
use unic_langid::LanguageIdentifier;

pub use asynchronous::GenerateBundles;
pub use synchronous::GenerateBundlesSync;

pub type FluentResourceSet = Vec<Rc<FluentResource>>;

#[derive(Default)]
struct Shared<P, B> {
    sources: Mutex<Vec<Vec<FileSource>>>,
    provider: P,
    bundle_adapter: Option<B>,
}

pub struct L10nRegistryLocked<'a, B> {
    lock: MutexGuard<'a, Vec<Vec<FileSource>>>,
    bundle_adapter: Option<&'a B>,
}

impl<'a, B> L10nRegistryLocked<'a, B> {
    pub fn iter(&self, metasource: usize) -> impl Iterator<Item = &FileSource> {
        self.lock
            .get(metasource)
            .expect("Index out-of-range")
            .iter()
    }

    pub fn number_of_metasources(&self) -> usize {
        self.lock.len()
    }

    pub fn metasource_len(&self, metasource: usize) -> usize {
        self.lock.get(metasource).expect("Index out-of-range").len()
    }

    pub fn source_idx(&self, metasource: usize, index: usize) -> &FileSource {
        let source_idx = self.metasource_len(metasource) - 1 - index;
        self.lock[metasource]
            .get(source_idx)
            .expect("Index out-of-range")
    }

    pub fn get_source(&self, metasource: usize, name: &str) -> Option<&FileSource> {
        self.lock
            .get(metasource)
            .expect("Index out-of-range")
            .iter()
            .find(|&source| source.name == name)
    }

    pub fn generate_sources_for_file<'l>(
        &'l self,
        metasource: usize,
        langid: &'l LanguageIdentifier,
        resource_id: &'l ResourceId,
    ) -> impl Iterator<Item = &FileSource> {
        self.iter(metasource)
            .filter(move |source| source.has_file(langid, resource_id) != Some(false))
    }
}

pub trait BundleAdapter {
    fn adapt_bundle(&self, bundle: &mut FluentBundle);
}

#[derive(Clone)]
pub struct L10nRegistry<P, B> {
    shared: Rc<Shared<P, B>>,
}

impl<P, B> L10nRegistry<P, B> {
    pub fn with_provider(provider: P) -> Self {
        Self {
            shared: Rc::new(Shared {
                sources: Default::default(),
                provider,
                bundle_adapter: None,
            }),
        }
    }

    pub fn set_adapt_bundle(&mut self, bundle_adapter: B) -> Result<(), L10nRegistrySetupError>
    where
        B: BundleAdapter,
    {
        let shared = Rc::get_mut(&mut self.shared).ok_or(L10nRegistrySetupError::RegistryLocked)?;
        shared.bundle_adapter = Some(bundle_adapter);
        Ok(())
    }

    pub fn lock(&self) -> L10nRegistryLocked<'_, B> {
        L10nRegistryLocked {
            // The lock() method only fails here if another thread has panicked
            // while holding the lock. In this case, we'll propagate the panic
            // as well. It's not clear what the recovery strategy would be for
            // us to deal with a panic in another thread.
            lock: self.shared.sources.lock().unwrap(),
            bundle_adapter: self.shared.bundle_adapter.as_ref(),
        }
    }

    pub fn register_sources(
        &self,
        new_sources: Vec<FileSource>,
    ) -> Result<(), L10nRegistrySetupError> {
        let mut sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;

        for new_source in new_sources {
            if let Some(metasource) = sources
                .iter_mut()
                .find(|source| source[0].metasource == new_source.metasource)
            {
                metasource.push(new_source);
            } else {
                sources.push(vec![new_source]);
            }
        }
        Ok(())
    }

    pub fn update_sources(
        &self,
        upd_sources: Vec<FileSource>,
    ) -> Result<(), L10nRegistrySetupError> {
        let mut sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;

        for upd_source in upd_sources {
            if let Some(metasource) = sources
                .iter_mut()
                .find(|source| source[0].metasource == upd_source.metasource)
            {
                if let Some(idx) = metasource.iter().position(|source| *source == upd_source) {
                    *metasource.get_mut(idx).unwrap() = upd_source;
                } else {
                    return Err(L10nRegistrySetupError::MissingSource {
                        name: upd_source.name,
                    });
                }
            }
        }
        Ok(())
    }

    pub fn remove_sources<S>(&self, del_sources: Vec<S>) -> Result<(), L10nRegistrySetupError>
    where
        S: ToString,
    {
        let mut sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;
        let del_sources: Vec<String> = del_sources.into_iter().map(|s| s.to_string()).collect();

        for metasource in sources.iter_mut() {
            metasource.retain(|source| !del_sources.contains(&source.name));
        }

        sources.retain(|metasource| !metasource.is_empty());

        Ok(())
    }

    pub fn clear_sources(&self) -> Result<(), L10nRegistrySetupError> {
        let mut sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;
        sources.clear();
        Ok(())
    }

    pub fn get_source_names(&self) -> Result<Vec<String>, L10nRegistrySetupError> {
        let sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;
        Ok(sources.iter().flatten().map(|s| s.name.clone()).collect())
    }

    pub fn has_source(&self, name: &str) -> Result<bool, L10nRegistrySetupError> {
        let sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;
        Ok(sources.iter().flatten().any(|source| source.name == name))
    }

    pub fn get_source(&self, name: &str) -> Result<Option<FileSource>, L10nRegistrySetupError> {
        let sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;
        Ok(sources
            .iter()
            .flatten()
            .find(|source| source.name == name)
            .cloned())
    }
    pub fn get_available_locales(&self) -> Result<Vec<LanguageIdentifier>, L10nRegistrySetupError> {
        let sources = self
            .shared
            .sources
            .try_lock()
            .map_err(|_| L10nRegistrySetupError::RegistryLocked)?;
        let mut result = HashSet::new();
        for source in sources.iter().flatten() {
            for locale in source.locales() {
                result.insert(locale);
            }
        }
        Ok(result.into_iter().map(|l| l.to_owned()).collect())
    }
}

impl<P, B> BundleGenerator for L10nRegistry<P, B>
where
    P: ErrorReporter + Clone,
    B: BundleAdapter + Clone,
{
    type Resource = Rc<FluentResource>;
    type Iter = GenerateBundlesSync<P, B>;
    type Stream = GenerateBundles<P, B>;
    type LocalesIter = std::vec::IntoIter<LanguageIdentifier>;

    fn bundles_iter(
        &self,
        locales: Self::LocalesIter,
        resource_ids: Vec<ResourceId>,
    ) -> Self::Iter {
        let resource_ids = resource_ids.into_iter().collect();
        self.generate_bundles_sync(locales, resource_ids)
    }

    fn bundles_stream(
        &self,
        locales: Self::LocalesIter,
        resource_ids: Vec<ResourceId>,
    ) -> Self::Stream {
        let resource_ids = resource_ids.into_iter().collect();
        self.generate_bundles(locales, resource_ids)
    }
}
