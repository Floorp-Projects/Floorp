use fluent_fallback::types::{ResourceType, ToResourceId};
use futures::future::join_all;
use l10nregistry_tests::TestFileFetcher;
use unic_langid::LanguageIdentifier;

static FTL_RESOURCE_PRESENT: &str = "toolkit/global/textActions.ftl";
static FTL_RESOURCE_MISSING: &str = "missing.ftl";

#[test]
fn test_fetch_sync() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let file = fs1.fetch_file_sync(&en_us, &FTL_RESOURCE_PRESENT.into(), false);
    assert!(file.is_some());
    assert!(!file.is_none());
    assert!(!file.is_required_and_missing());

    let file = fs1.fetch_file_sync(
        &en_us,
        &FTL_RESOURCE_PRESENT.to_resource_id(ResourceType::Required),
        false,
    );
    assert!(file.is_some());
    assert!(!file.is_none());
    assert!(!file.is_required_and_missing());

    let file = fs1.fetch_file_sync(
        &en_us,
        &FTL_RESOURCE_PRESENT.to_resource_id(ResourceType::Optional),
        false,
    );
    assert!(file.is_some());
    assert!(!file.is_none());
    assert!(!file.is_required_and_missing());

    let file = fs1.fetch_file_sync(&en_us, &FTL_RESOURCE_MISSING.into(), false);
    assert!(!file.is_some());
    assert!(file.is_none());
    assert!(file.is_required_and_missing());

    let file = fs1.fetch_file_sync(
        &en_us,
        &FTL_RESOURCE_MISSING.to_resource_id(ResourceType::Required),
        false,
    );
    assert!(!file.is_some());
    assert!(file.is_none());
    assert!(file.is_required_and_missing());

    let file = fs1.fetch_file_sync(
        &en_us,
        &FTL_RESOURCE_MISSING.to_resource_id(ResourceType::Optional),
        false,
    );
    assert!(!file.is_some());
    assert!(file.is_none());
    assert!(!file.is_required_and_missing());
}

#[tokio::test]
async fn test_fetch_async() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let file = fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into()).await;
    assert!(file.is_some());
    assert!(!file.is_none());
    assert!(!file.is_required_and_missing());

    let file = fs1
        .fetch_file(
            &en_us,
            &FTL_RESOURCE_PRESENT.to_resource_id(ResourceType::Required),
        )
        .await;
    assert!(file.is_some());
    assert!(!file.is_none());
    assert!(!file.is_required_and_missing());

    let file = fs1
        .fetch_file(
            &en_us,
            &FTL_RESOURCE_PRESENT.to_resource_id(ResourceType::Optional),
        )
        .await;
    assert!(file.is_some());
    assert!(!file.is_none());
    assert!(!file.is_required_and_missing());

    let file = fs1.fetch_file(&en_us, &FTL_RESOURCE_MISSING.into()).await;
    assert!(!file.is_some());
    assert!(file.is_none());
    assert!(file.is_required_and_missing());

    let file = fs1
        .fetch_file(
            &en_us,
            &FTL_RESOURCE_MISSING.to_resource_id(ResourceType::Required),
        )
        .await;
    assert!(!file.is_some());
    assert!(file.is_none());
    assert!(file.is_required_and_missing());

    let file = fs1
        .fetch_file(
            &en_us,
            &FTL_RESOURCE_MISSING.to_resource_id(ResourceType::Optional),
        )
        .await;
    assert!(!file.is_some());
    assert!(file.is_none());
    assert!(!file.is_required_and_missing());
}

#[tokio::test]
async fn test_fetch_sync_2_async() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    assert!(fs1
        .fetch_file_sync(&en_us, &FTL_RESOURCE_PRESENT.into(), false)
        .is_some());
    assert!(fs1
        .fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into())
        .await
        .is_some());
    assert!(fs1
        .fetch_file_sync(&en_us, &FTL_RESOURCE_PRESENT.into(), false)
        .is_some());
}

#[tokio::test]
async fn test_fetch_async_2_sync() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    assert!(fs1
        .fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into())
        .await
        .is_some());
    assert!(fs1
        .fetch_file_sync(&en_us, &FTL_RESOURCE_PRESENT.into(), false)
        .is_some());
}

#[test]
fn test_fetch_has_value_required_sync() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let path = FTL_RESOURCE_PRESENT.into();
    let path_missing = FTL_RESOURCE_MISSING.into();

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    assert_eq!(fs1.has_file(&en_us, &path), None);
    assert!(fs1.fetch_file_sync(&en_us, &path, false).is_some());
    assert_eq!(fs1.has_file(&en_us, &path), Some(true));

    assert_eq!(fs1.has_file(&en_us, &path_missing), None);
    assert!(fs1.fetch_file_sync(&en_us, &path_missing, false).is_none());
    assert_eq!(fs1.has_file(&en_us, &path_missing), Some(false));
}

#[test]
fn test_fetch_has_value_optional_sync() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let path = FTL_RESOURCE_PRESENT.to_resource_id(ResourceType::Optional);
    let path_missing = FTL_RESOURCE_MISSING.to_resource_id(ResourceType::Optional);

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    assert_eq!(fs1.has_file(&en_us, &path), None);
    assert!(fs1.fetch_file_sync(&en_us, &path, false).is_some());
    assert_eq!(fs1.has_file(&en_us, &path), Some(true));

    assert_eq!(fs1.has_file(&en_us, &path_missing), None);
    assert!(fs1.fetch_file_sync(&en_us, &path_missing, false).is_none());
    assert_eq!(fs1.has_file(&en_us, &path_missing), Some(false));
}

#[tokio::test]
async fn test_fetch_has_value_required_async() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let path = FTL_RESOURCE_PRESENT.into();
    let path_missing = FTL_RESOURCE_MISSING.into();

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    assert_eq!(fs1.has_file(&en_us, &path), None);
    assert!(fs1.fetch_file(&en_us, &path).await.is_some());
    println!("Completed");
    assert_eq!(fs1.has_file(&en_us, &path), Some(true));

    assert_eq!(fs1.has_file(&en_us, &path_missing), None);

    assert!(fs1.fetch_file(&en_us, &path_missing).await.is_none());
    assert!(fs1
        .fetch_file(&en_us, &path_missing)
        .await
        .is_required_and_missing());

    assert_eq!(fs1.has_file(&en_us, &path_missing), Some(false));

    assert!(fs1.fetch_file_sync(&en_us, &path_missing, false).is_none());
    assert!(fs1
        .fetch_file_sync(&en_us, &path_missing, false)
        .is_required_and_missing());
}

#[tokio::test]
async fn test_fetch_has_value_optional_async() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let path = FTL_RESOURCE_PRESENT.to_resource_id(ResourceType::Optional);
    let path_missing = FTL_RESOURCE_MISSING.to_resource_id(ResourceType::Optional);

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    assert_eq!(fs1.has_file(&en_us, &path), None);
    assert!(fs1.fetch_file(&en_us, &path).await.is_some());
    println!("Completed");
    assert_eq!(fs1.has_file(&en_us, &path), Some(true));

    assert_eq!(fs1.has_file(&en_us, &path_missing), None);
    assert!(fs1.fetch_file(&en_us, &path_missing).await.is_none());
    assert!(!fs1
        .fetch_file(&en_us, &path_missing)
        .await
        .is_required_and_missing());

    assert_eq!(fs1.has_file(&en_us, &path_missing), Some(false));

    assert!(fs1.fetch_file_sync(&en_us, &path_missing, false).is_none());
    assert!(!fs1
        .fetch_file_sync(&en_us, &path_missing, false)
        .is_required_and_missing());
}

#[tokio::test]
async fn test_fetch_async_consecutive() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();

    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let results = join_all(vec![
        fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into()),
        fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into()),
    ])
    .await;
    assert!(results[0].is_some());
    assert!(results[1].is_some());

    assert!(fs1
        .fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into())
        .await
        .is_some());
}

#[test]
fn test_indexed() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let path = FTL_RESOURCE_PRESENT;
    let path_missing = FTL_RESOURCE_MISSING;

    let fs1 = fetcher.get_test_file_source_with_index(
        "toolkit",
        None,
        vec![en_us.clone()],
        "toolkit/{locale}/",
        vec!["toolkit/en-US/toolkit/global/textActions.ftl"],
    );

    assert_eq!(fs1.has_file(&en_us, &path.into()), Some(true));
    assert!(fs1.fetch_file_sync(&en_us, &path.into(), false).is_some());
    assert_eq!(fs1.has_file(&en_us, &path.into()), Some(true));

    assert_eq!(fs1.has_file(&en_us, &path_missing.into()), Some(false));
    assert!(fs1
        .fetch_file_sync(&en_us, &path_missing.into(), false)
        .is_none());
    assert_eq!(fs1.has_file(&en_us, &path_missing.into()), Some(false));
}
