use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use futures::stream::StreamExt;
use l10nregistry::source::ResourceId;
use l10nregistry_tests::{FileSource, RegistrySetup, TestFileFetcher};
use unic_langid::LanguageIdentifier;

fn get_paths() -> Vec<ResourceId> {
    let paths: Vec<&'static str> = vec![
        "branding/brand.ftl",
        "browser/sanitize.ftl",
        "browser/preferences/blocklists.ftl",
        "browser/preferences/colors.ftl",
        "browser/preferences/selectBookmark.ftl",
        "browser/preferences/connection.ftl",
        "browser/preferences/addEngine.ftl",
        "browser/preferences/siteDataSettings.ftl",
        "browser/preferences/fonts.ftl",
        "browser/preferences/languages.ftl",
        "browser/preferences/preferences.ftl",
        "security/certificates/certManager.ftl",
        "security/certificates/deviceManager.ftl",
        "toolkit/global/textActions.ftl",
        "toolkit/printing/printUI.ftl",
        "toolkit/updates/history.ftl",
        "toolkit/featuregates/features.ftl",
    ];

    paths.into_iter().map(ResourceId::from).collect()
}

fn registry_bench(c: &mut Criterion) {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let mut group = c.benchmark_group("non-metasource");

    let setup = RegistrySetup::new(
        "test",
        vec![
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
            FileSource::new("toolkit", None, vec![en_us.clone()], "toolkit/{locale}/"),
            FileSource::new("browser", None, vec![en_us.clone()], "browser/{locale}/"),
        ],
        vec![en_us.clone()],
    );
    let fetcher = TestFileFetcher::new();
    let (_, reg) = fetcher.get_registry_and_environment(setup);

    group.bench_function(&format!("serial",), |b| {
        b.iter(|| {
            let lang_ids = vec![en_us.clone()];
            let mut i = reg.generate_bundles_sync(lang_ids.into_iter(), get_paths());
            while let Some(_) = i.next() {}
        })
    });

    let rt = tokio::runtime::Runtime::new().unwrap();
    group.bench_function(&format!("parallel",), |b| {
        b.iter(|| {
            let lang_ids = vec![en_us.clone()];
            let mut i = reg.generate_bundles(lang_ids.into_iter(), get_paths());
            rt.block_on(async { while let Some(_) = i.as_mut().unwrap().next().await {} });
        })
    });

    group.finish();
}

fn registry_metasource_bench(c: &mut Criterion) {
    let en_us: LanguageIdentifier = "en-US".parse().unwrap();
    let mut group = c.benchmark_group("metasource");

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

    group.bench_function(&format!("serial",), |b| {
        b.iter(|| {
            let lang_ids = vec![en_us.clone()];
            let mut i = reg.generate_bundles_sync(lang_ids.into_iter(), get_paths());
            while let Some(_) = i.next() {}
        })
    });

    let rt = tokio::runtime::Runtime::new().unwrap();
    group.bench_function(&format!("parallel",), |b| {
        b.iter(|| {
            let lang_ids = vec![en_us.clone()];
            let mut i = reg.generate_bundles(lang_ids.into_iter(), get_paths());
            rt.block_on(async { while let Some(_) = i.as_mut().unwrap().next().await {} });
        })
    });

    group.finish();
}

criterion_group!(
    name = benches;
    config = Criterion::default().sample_size(10);
    targets = registry_bench, registry_metasource_bench
);
criterion_main!(benches);
