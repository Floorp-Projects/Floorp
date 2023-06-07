use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use fluent_testing::get_scenarios;
use l10nregistry_tests::TestFileFetcher;

use unic_langid::LanguageIdentifier;

fn preferences_bench(c: &mut Criterion) {
    let fetcher = TestFileFetcher::new();

    let mut group = c.benchmark_group("registry/scenarios");

    for scenario in get_scenarios() {
        let res_ids = scenario.res_ids.clone();

        let locales: Vec<LanguageIdentifier> = scenario
            .locales
            .iter()
            .map(|l| l.parse().unwrap())
            .collect();

        group.bench_function(format!("{}/sync/first_bundle", scenario.name), |b| {
            b.iter(|| {
                let reg = fetcher.get_registry(&scenario);
                let mut bundles =
                    reg.generate_bundles_sync(locales.clone().into_iter(), res_ids.clone());
                for _ in 0..locales.len() {
                    if bundles.next().is_some() {
                        break;
                    }
                }
            })
        });

        #[cfg(feature = "tokio")]
        {
            use futures::stream::StreamExt;

            let rt = tokio::runtime::Runtime::new().unwrap();

            group.bench_function(&format!("{}/async/first_bundle", scenario.name), |b| {
                b.iter(|| {
                    rt.block_on(async {
                        let reg = fetcher.get_registry(&scenario);

                        let mut bundles =
                            reg.generate_bundles(locales.clone().into_iter(), res_ids.clone());
                        for _ in 0..locales.len() {
                            if bundles.next().await.is_some() {
                                break;
                            }
                        }
                    });
                })
            });
        }
    }

    group.finish();
}

criterion_group!(benches, preferences_bench);
criterion_main!(benches);
