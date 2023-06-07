use l10nregistry_tests::TestFileFetcher;
use unic_langid::LanguageIdentifier;

static FTL_RESOURCE_PRESENT: &str = "toolkit/global/textActions.ftl";
static FTL_RESOURCE_MISSING: &str = "missing.ftl";

#[tokio::test]
async fn file_source_fetch() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let file = fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into()).await;
    assert!(file.is_some());
}

#[tokio::test]
async fn file_source_fetch_missing() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let file = fs1.fetch_file(&en_us, &FTL_RESOURCE_MISSING.into()).await;
    assert!(file.is_none());
}

#[tokio::test]
async fn file_source_already_loaded() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let file = fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into()).await;
    assert!(file.is_some());
    let file = fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into()).await;
    assert!(file.is_some());
}

#[tokio::test]
async fn file_source_concurrent() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let file1 = fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into());
    let file2 = fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into());
    assert!(file1.await.is_some());
    assert!(file2.await.is_some());
}

#[test]
fn file_source_sync_after_async_fail() {
    let fetcher = TestFileFetcher::new();
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let fs1 =
        fetcher.get_test_file_source("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/");

    let _ = fs1.fetch_file(&en_us, &FTL_RESOURCE_PRESENT.into());
    let file2 = fs1.fetch_file_sync(&en_us, &FTL_RESOURCE_PRESENT.into(), true);
    assert!(file2.is_some());
}
