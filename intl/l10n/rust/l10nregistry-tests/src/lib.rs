use l10nregistry::env::ErrorReporter;
use l10nregistry::errors::L10nRegistryError;
use l10nregistry::fluent::FluentBundle;
use l10nregistry::registry::BundleAdapter;
use l10nregistry::registry::L10nRegistry;
use l10nregistry::source::FileFetcher;
use async_trait::async_trait;
use fluent_fallback::{env::LocalesProvider, types::ResourceId};
use fluent_testing::MockFileSystem;
use std::cell::RefCell;
use std::rc::Rc;
use unic_langid::LanguageIdentifier;

pub mod solver;

pub struct RegistrySetup {
    pub name: String,
    pub file_sources: Vec<FileSource>,
    pub locales: Vec<LanguageIdentifier>,
}

pub struct FileSource {
    pub name: String,
    pub metasource: String,
    pub locales: Vec<LanguageIdentifier>,
    pub path_scheme: String,
}

#[derive(Clone)]
pub struct MockBundleAdapter;

impl BundleAdapter for MockBundleAdapter {
    fn adapt_bundle(&self, _bundle: &mut FluentBundle) {}
}

impl FileSource {
    pub fn new<S>(
        name: S,
        metasource: Option<S>,
        locales: Vec<LanguageIdentifier>,
        path_scheme: S,
    ) -> Self
    where
        S: ToString,
    {
        let metasource = match metasource {
            Some(s) => s.to_string(),
            None => String::default(),
        };

        Self {
            name: name.to_string(),
            metasource,
            locales,
            path_scheme: path_scheme.to_string(),
        }
    }
}

impl RegistrySetup {
    pub fn new(
        name: &str,
        file_sources: Vec<FileSource>,
        locales: Vec<LanguageIdentifier>,
    ) -> Self {
        Self {
            name: name.to_string(),
            file_sources,
            locales,
        }
    }
}

impl From<fluent_testing::scenarios::structs::Scenario> for RegistrySetup {
    fn from(s: fluent_testing::scenarios::structs::Scenario) -> Self {
        Self {
            name: s.name,
            file_sources: s
                .file_sources
                .into_iter()
                .map(|source| {
                    FileSource::new(
                        source.name,
                        None,
                        source
                            .locales
                            .into_iter()
                            .map(|l| l.parse().unwrap())
                            .collect(),
                        source.path_scheme,
                    )
                })
                .collect(),
            locales: s
                .locales
                .into_iter()
                .map(|loc| loc.parse().unwrap())
                .collect(),
        }
    }
}

impl From<&fluent_testing::scenarios::structs::Scenario> for RegistrySetup {
    fn from(s: &fluent_testing::scenarios::structs::Scenario) -> Self {
        Self {
            name: s.name.clone(),
            file_sources: s
                .file_sources
                .iter()
                .map(|source| {
                    FileSource::new(
                        source.name.clone(),
                        None,
                        source.locales.iter().map(|l| l.parse().unwrap()).collect(),
                        source.path_scheme.clone(),
                    )
                })
                .collect(),
            locales: s.locales.iter().map(|loc| loc.parse().unwrap()).collect(),
        }
    }
}

#[derive(Default)]
struct InnerFileFetcher {
    fs: MockFileSystem,
}

#[derive(Clone)]
pub struct TestFileFetcher {
    inner: Rc<InnerFileFetcher>,
}

impl TestFileFetcher {
    pub fn new() -> Self {
        Self {
            inner: Rc::new(InnerFileFetcher::default()),
        }
    }

    pub fn get_test_file_source(
        &self,
        name: &str,
        metasource: Option<String>,
        locales: Vec<LanguageIdentifier>,
        path: &str,
    ) -> l10nregistry::source::FileSource {
        l10nregistry::source::FileSource::new(
            name.to_string(),
            metasource,
            locales,
            path.to_string(),
            Default::default(),
            self.clone(),
        )
    }

    pub fn get_test_file_source_with_index(
        &self,
        name: &str,
        metasource: Option<String>,
        locales: Vec<LanguageIdentifier>,
        path: &str,
        index: Vec<&str>,
    ) -> l10nregistry::source::FileSource {
        l10nregistry::source::FileSource::new_with_index(
            name.to_string(),
            metasource,
            locales,
            path.to_string(),
            Default::default(),
            self.clone(),
            index.into_iter().map(|s| s.to_string()).collect(),
        )
    }

    pub fn get_registry<S>(&self, setup: S) -> L10nRegistry<TestEnvironment, MockBundleAdapter>
    where
        S: Into<RegistrySetup>,
    {
        self.get_registry_and_environment(setup).1
    }

    pub fn get_registry_and_environment<S>(
        &self,
        setup: S,
    ) -> (
        TestEnvironment,
        L10nRegistry<TestEnvironment, MockBundleAdapter>,
    )
    where
        S: Into<RegistrySetup>,
    {
        let setup: RegistrySetup = setup.into();
        let provider = TestEnvironment::new(setup.locales);

        let reg = L10nRegistry::with_provider(provider.clone());
        let sources = setup
            .file_sources
            .into_iter()
            .map(|source| {
                let mut s = self.get_test_file_source(
                    &source.name,
                    Some(source.metasource),
                    source.locales,
                    &source.path_scheme,
                );
                s.set_reporter(provider.clone());
                s
            })
            .collect();
        reg.register_sources(sources).unwrap();
        (provider, reg)
    }

    pub fn get_registry_and_environment_with_adapter<S, B>(
        &self,
        setup: S,
        bundle_adapter: B,
    ) -> (TestEnvironment, L10nRegistry<TestEnvironment, B>)
    where
        S: Into<RegistrySetup>,
        B: BundleAdapter,
    {
        let setup: RegistrySetup = setup.into();
        let provider = TestEnvironment::new(setup.locales);

        let mut reg = L10nRegistry::with_provider(provider.clone());
        let sources = setup
            .file_sources
            .into_iter()
            .map(|source| {
                let mut s = self.get_test_file_source(
                    &source.name,
                    None,
                    source.locales,
                    &source.path_scheme,
                );
                s.set_reporter(provider.clone());
                s
            })
            .collect();
        reg.register_sources(sources).unwrap();
        reg.set_bundle_adapter(bundle_adapter)
            .expect("Failed to set bundle adapter.");
        (provider, reg)
    }
}

#[async_trait(?Send)]
impl FileFetcher for TestFileFetcher {
    fn fetch_sync(&self, resource_id: &ResourceId) -> std::io::Result<String> {
        self.inner.fs.get_test_file_sync(&resource_id.value)
    }

    async fn fetch(&self, resource_id: &ResourceId) -> std::io::Result<String> {
        self.inner.fs.get_test_file_async(&resource_id.value).await
    }
}

pub enum ErrorStrategy {
    Panic,
    Report,
    Nothing,
}

pub struct InnerTestEnvironment {
    locales: Vec<LanguageIdentifier>,
    errors: Vec<L10nRegistryError>,
    error_strategy: ErrorStrategy,
}

#[derive(Clone)]
pub struct TestEnvironment {
    inner: Rc<RefCell<InnerTestEnvironment>>,
}

impl TestEnvironment {
    pub fn new(locales: Vec<LanguageIdentifier>) -> Self {
        Self {
            inner: Rc::new(RefCell::new(InnerTestEnvironment {
                locales,
                errors: vec![],
                error_strategy: ErrorStrategy::Report,
            })),
        }
    }

    pub fn set_locales(&self, locales: Vec<LanguageIdentifier>) {
        self.inner.borrow_mut().locales = locales;
    }

    pub fn errors(&self) -> Vec<L10nRegistryError> {
        self.inner.borrow().errors.clone()
    }

    pub fn clear_errors(&self) {
        self.inner.borrow_mut().errors.clear()
    }
}

impl LocalesProvider for TestEnvironment {
    type Iter = std::vec::IntoIter<LanguageIdentifier>;

    fn locales(&self) -> Self::Iter {
        self.inner.borrow().locales.clone().into_iter()
    }
}

impl ErrorReporter for TestEnvironment {
    fn report_errors(&self, errors: Vec<L10nRegistryError>) {
        match self.inner.borrow().error_strategy {
            ErrorStrategy::Panic => {
                panic!("Errors: {:#?}", errors);
            }
            ErrorStrategy::Report => {
                #[cfg(test)] // Don't let printing affect benchmarks
                eprintln!("Errors: {:#?}", errors);
            }
            ErrorStrategy::Nothing => {}
        }
        self.inner.borrow_mut().errors.extend(errors);
    }
}
