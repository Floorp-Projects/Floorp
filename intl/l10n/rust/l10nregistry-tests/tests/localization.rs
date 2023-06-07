use std::borrow::Cow;

use fluent_fallback::{
    env::LocalesProvider,
    types::{L10nKey, ResourceId},
    Localization,
};
use l10nregistry_tests::{
    FileSource, MockBundleAdapter, RegistrySetup, TestEnvironment, TestFileFetcher,
};
use serial_test::serial;
use unic_langid::{langid, LanguageIdentifier};

type L10nRegistry = l10nregistry::registry::L10nRegistry<TestEnvironment, MockBundleAdapter>;

static LOCALES: &[LanguageIdentifier] = &[langid!("pl"), langid!("en-US")];
static mut FILE_FETCHER: Option<TestFileFetcher> = None;
static mut L10N_REGISTRY: Option<L10nRegistry> = None;

const FTL_RESOURCE: &str = "toolkit/updates/history.ftl";
const L10N_ID_PL_EN: (&str, Option<&str>) = ("history-title", Some("Historia aktualizacji"));
const L10N_ID_MISSING: (&str, Option<&str>) = ("missing-id", None);
const L10N_ID_ONLY_EN: (&str, Option<&str>) = (
    "history-intro",
    Some("The following updates have been installed"),
);

fn get_file_fetcher() -> &'static TestFileFetcher {
    let fetcher: &mut Option<TestFileFetcher> = unsafe { &mut FILE_FETCHER };

    fetcher.get_or_insert_with(|| TestFileFetcher::new())
}

fn get_l10n_registry() -> &'static L10nRegistry {
    let reg: &mut Option<L10nRegistry> = unsafe { &mut L10N_REGISTRY };

    reg.get_or_insert_with(|| {
        let fetcher = get_file_fetcher();
        let setup = RegistrySetup::new(
            "test",
            vec![
                FileSource::new(
                    "toolkit",
                    None,
                    get_app_locales().to_vec(),
                    "toolkit/{locale}/",
                ),
                FileSource::new(
                    "browser",
                    None,
                    get_app_locales().to_vec(),
                    "browser/{locale}/",
                ),
            ],
            get_app_locales().to_vec(),
        );
        fetcher.get_registry_and_environment(setup).1
    })
}

fn get_app_locales() -> &'static [LanguageIdentifier] {
    LOCALES
}

struct LocalesService;

impl LocalesProvider for LocalesService {
    type Iter = std::vec::IntoIter<LanguageIdentifier>;

    fn locales(&self) -> Self::Iter {
        get_app_locales().to_vec().into_iter()
    }
}

fn sync_localization(
    reg: &'static L10nRegistry,
    res_ids: Vec<ResourceId>,
) -> Localization<L10nRegistry, LocalesService> {
    Localization::with_env(res_ids, true, LocalesService, reg.clone())
}

fn async_localization(
    reg: &'static L10nRegistry,
    res_ids: Vec<ResourceId>,
) -> Localization<L10nRegistry, LocalesService> {
    Localization::with_env(res_ids, false, LocalesService, reg.clone())
}

fn setup_sync_test() -> Localization<L10nRegistry, LocalesService> {
    sync_localization(get_l10n_registry(), vec![FTL_RESOURCE.into()])
}

fn setup_async_test() -> Localization<L10nRegistry, LocalesService> {
    async_localization(get_l10n_registry(), vec![FTL_RESOURCE.into()])
}

#[test]
#[serial]
fn localization_format_value_sync() {
    let loc = setup_sync_test();
    let bundles = loc.bundles();
    let mut errors = vec![];

    for query in &[L10N_ID_PL_EN, L10N_ID_MISSING, L10N_ID_ONLY_EN] {
        let value = bundles
            .format_value_sync(query.0, None, &mut errors)
            .unwrap();
        assert_eq!(value, query.1.map(|s| Cow::Borrowed(s)));
    }

    assert_eq!(errors.len(), 4);
}

#[test]
#[serial]
fn localization_format_values_sync() {
    let loc = setup_sync_test();
    let bundles = loc.bundles();
    let mut errors = vec![];

    let ids = &[L10N_ID_PL_EN, L10N_ID_MISSING, L10N_ID_ONLY_EN];
    let keys = ids
        .iter()
        .map(|query| L10nKey {
            id: query.0.into(),
            args: None,
        })
        .collect::<Vec<_>>();

    let values = bundles.format_values_sync(&keys, &mut errors).unwrap();

    assert_eq!(values.len(), ids.len());

    for (value, query) in values.iter().zip(ids) {
        if let Some(expected) = query.1 {
            assert_eq!(*value, Some(Cow::Borrowed(expected)));
        }
    }
    assert_eq!(errors.len(), 4);
}

#[tokio::test]
#[serial]
async fn localization_format_value_async() {
    let loc = setup_async_test();
    let bundles = loc.bundles();
    let mut errors = vec![];

    for query in &[L10N_ID_PL_EN, L10N_ID_MISSING, L10N_ID_ONLY_EN] {
        let value = bundles.format_value(query.0, None, &mut errors).await;
        if let Some(expected) = query.1 {
            assert_eq!(value, Some(Cow::Borrowed(expected)));
        }
    }
}

#[tokio::test]
#[serial]
async fn localization_format_values_async() {
    let loc = setup_async_test();
    let bundles = loc.bundles();
    let mut errors = vec![];

    let ids = &[L10N_ID_PL_EN, L10N_ID_MISSING, L10N_ID_ONLY_EN];
    let keys = ids
        .iter()
        .map(|query| L10nKey {
            id: query.0.into(),
            args: None,
        })
        .collect::<Vec<_>>();

    let values = bundles.format_values(&keys, &mut errors).await;

    assert_eq!(values.len(), ids.len());

    for (value, query) in values.iter().zip(ids) {
        if let Some(expected) = query.1 {
            assert_eq!(*value, Some(Cow::Borrowed(expected)));
        }
    }
}

#[tokio::test]
#[serial]
async fn localization_upgrade() {
    let mut loc = setup_sync_test();
    let bundles = loc.bundles();
    let mut errors = vec![];
    let value = bundles
        .format_value_sync(L10N_ID_PL_EN.0, None, &mut errors)
        .unwrap();
    assert_eq!(value, L10N_ID_PL_EN.1.map(|s| Cow::Borrowed(s)));

    loc.set_async();
    let bundles = loc.bundles();
    let value = bundles
        .format_value(L10N_ID_PL_EN.0, None, &mut errors)
        .await;
    assert_eq!(value, L10N_ID_PL_EN.1.map(|s| Cow::Borrowed(s)));
}
