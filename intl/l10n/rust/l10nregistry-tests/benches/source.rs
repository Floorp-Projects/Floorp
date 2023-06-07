use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use fluent_testing::get_scenarios;
use l10nregistry_tests::TestFileFetcher;

use unic_langid::LanguageIdentifier;

fn get_locales<S>(input: &[S]) -> Vec<LanguageIdentifier>
where
    S: AsRef<str>,
{
    input.iter().map(|s| s.as_ref().parse().unwrap()).collect()
}

fn source_bench(c: &mut Criterion) {
    let fetcher = TestFileFetcher::new();

    let mut group = c.benchmark_group("source/scenarios");

    for scenario in get_scenarios() {
        let res_ids = scenario.res_ids.clone();

        let locales: Vec<LanguageIdentifier> = get_locales(&scenario.locales);

        let sources: Vec<_> = scenario
            .file_sources
            .iter()
            .map(|s| {
                fetcher.get_test_file_source(&s.name, None, get_locales(&s.locales), &s.path_scheme)
            })
            .collect();

        group.bench_function(format!("{}/has_file", scenario.name), |b| {
            b.iter(|| {
                for source in &sources {
                    for res_id in &res_ids {
                        source.has_file(&locales[0], &res_id);
                    }
                }
            })
        });

        group.bench_function(format!("{}/sync/fetch_file_sync", scenario.name), |b| {
            b.iter(|| {
                for source in &sources {
                    for res_id in &res_ids {
                        source.fetch_file_sync(&locales[0], &res_id, false);
                    }
                }
            })
        });
    }

    group.finish();
}

criterion_group!(benches, source_bench);
criterion_main!(benches);
