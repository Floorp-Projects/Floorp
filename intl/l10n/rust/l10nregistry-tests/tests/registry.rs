use l10nregistry_tests::{FileSource, RegistrySetup, TestFileFetcher};
use unic_langid::LanguageIdentifier;

static FTL_RESOURCE_TOOLKIT: &str = "toolkit/global/textActions.ftl";
static FTL_RESOURCE_BROWSER: &str = "branding/brand.ftl";

#[test]
fn test_get_sources_for_resource() {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    {
        let metasources = reg
            .try_borrow_metasources()
            .expect("Unable to borrow metasources.");

        let toolkit = metasources.file_source_by_name(0, "toolkit").unwrap();
        let browser = metasources.file_source_by_name(0, "browser").unwrap();
        let toolkit_resource_id = FTL_RESOURCE_TOOLKIT.into();

        let mut i = metasources.get_sources_for_resource(0, &en_us, &toolkit_resource_id);

        assert_eq!(i.next(), Some(toolkit));
        assert_eq!(i.next(), Some(browser));
        assert_eq!(i.next(), None);

        assert!(browser
            .fetch_file_sync(&en_us, &FTL_RESOURCE_TOOLKIT.into(), false)
            .is_none());

        let mut i = metasources.get_sources_for_resource(0, &en_us, &toolkit_resource_id);
        assert_eq!(i.next(), Some(toolkit));
        assert_eq!(i.next(), None);

        assert!(toolkit
            .fetch_file_sync(&en_us, &FTL_RESOURCE_TOOLKIT.into(), false)
            .is_some());

        let mut i = metasources.get_sources_for_resource(0, &en_us, &toolkit_resource_id);
        assert_eq!(i.next(), Some(toolkit));
        assert_eq!(i.next(), None);
    }
}

#[test]
fn test_generate_bundles_for_lang_sync() {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let mut i = reg.generate_bundles_for_lang_sync(en_us.clone(), paths);

    assert!(i.next().is_some());
    assert!(i.next().is_none());
}

#[test]
fn test_generate_bundles_sync() {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let lang_ids = vec![en_us];
    let mut i = reg.generate_bundles_sync(lang_ids.into_iter(), paths);

    assert!(i.next().is_some());
    assert!(i.next().is_none());
}

#[tokio::test]
async fn test_generate_bundles_for_lang() {
    use futures::stream::StreamExt;

    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let mut i = reg
        .generate_bundles_for_lang(en_us, paths)
        .expect("Failed to get GenerateBundles.");

    assert!(i.next().await.is_some());
    assert!(i.next().await.is_none());
}

#[tokio::test]
async fn test_generate_bundles() {
    use futures::stream::StreamExt;

    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let langs = vec![en_us];
    let mut i = reg
        .generate_bundles(langs.into_iter(), paths)
        .expect("Failed to get GenerateBundles.");

    assert!(i.next().await.is_some());
    assert!(i.next().await.is_none());
}

#[test]
fn test_manage_sources() {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    let lang_ids = vec![en_us];

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];

    let mut i = reg.generate_bundles_sync(lang_ids.clone().into_iter(), paths);

    assert!(i.next().is_some());
    assert!(i.next().is_none());

    reg.clone()
        .remove_sources(vec!["toolkit"])
        .expect("Failed to remove a source.");

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let mut i = reg.generate_bundles_sync(lang_ids.clone().into_iter(), paths);
    assert!(i.next().is_none());

    let paths = vec![FTL_RESOURCE_BROWSER.into()];
    let mut i = reg.generate_bundles_sync(lang_ids.clone().into_iter(), paths);
    assert!(i.next().is_some());
    assert!(i.next().is_none());

    reg.register_sources(vec![fetcher.get_test_file_source(
        "toolkit",
        None,
        lang_ids.clone(),
        "browser/{locale}/",
    )])
    .expect("Failed to register a source.");

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let mut i = reg.generate_bundles_sync(lang_ids.clone().into_iter(), paths);
    assert!(i.next().is_none());

    reg.update_sources(vec![fetcher.get_test_file_source(
        "toolkit",
        None,
        lang_ids.clone(),
        "toolkit/{locale}/",
    )])
    .expect("Failed to update a source.");

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let mut i = reg.generate_bundles_sync(lang_ids.clone().into_iter(), paths);
    assert!(i.next().is_some());
    assert!(i.next().is_none());
}

#[test]
fn test_generate_bundles_with_metasources_sync() {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new(
                "toolkit",
                Some("app"),
                vec![en_us.clone()],
                "toolkit/{locale}/",
            ),
            FileSource::new(
                "browser",
                Some("app"),
                vec![en_us.clone()],
                "browser/{locale}/",
            ),
            FileSource::new(
                "toolkit",
                Some("langpack"),
                vec![en_us.clone()],
                "toolkit/{locale}/",
            ),
            FileSource::new(
                "browser",
                Some("langpack"),
                vec![en_us.clone()],
                "browser/{locale}/",
            ),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let lang_ids = vec![en_us];
    let mut i = reg.generate_bundles_sync(lang_ids.into_iter(), paths);

    assert!(i.next().is_some());
    assert!(i.next().is_some());
    assert!(i.next().is_none());
}

#[tokio::test]
async fn test_generate_bundles_with_metasources() {
    use futures::stream::StreamExt;

    let en_us: LanguageIdentifier = "en-US".parse().unwrap();

    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new(
                "toolkit",
                Some("app"),
                vec![en_us.clone()],
                "toolkit/{locale}/",
            ),
            FileSource::new(
                "browser",
                Some("app"),
                vec![en_us.clone()],
                "browser/{locale}/",
            ),
            FileSource::new(
                "toolkit",
                Some("langpack"),
                vec![en_us.clone()],
                "toolkit/{locale}/",
            ),
            FileSource::new(
                "browser",
                Some("langpack"),
                vec![en_us.clone()],
                "browser/{locale}/",
            ),
        ],
        vec![en_us.clone()],
    );

    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    let paths = vec![FTL_RESOURCE_TOOLKIT.into(), FTL_RESOURCE_BROWSER.into()];
    let langs = vec![en_us];
    let mut i = reg
        .generate_bundles(langs.into_iter(), paths)
        .expect("Failed to get GenerateBundles.");

    assert!(i.next().await.is_some());
    assert!(i.next().await.is_some());
    assert!(i.next().await.is_none());
}
