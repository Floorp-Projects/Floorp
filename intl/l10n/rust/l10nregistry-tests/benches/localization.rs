use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use fluent_bundle::FluentArgs;
use fluent_fallback::{types::L10nKey, Localization};
use fluent_testing::get_scenarios;
use l10nregistry_tests::TestFileFetcher;

fn preferences_bench(c: &mut Criterion) {
    let fetcher = TestFileFetcher::new();

    let mut group = c.benchmark_group("localization/scenarios");

    for scenario in get_scenarios() {
        let res_ids = scenario.res_ids.clone();
        let l10n_keys: Vec<(String, Option<FluentArgs>)> = scenario
            .queries
            .iter()
            .map(|q| {
                (
                    q.input.id.clone(),
                    q.input.args.as_ref().map(|args| {
                        let mut result = FluentArgs::new();
                        for arg in args.as_slice() {
                            result.set(arg.id.clone(), arg.value.clone());
                        }
                        result
                    }),
                )
            })
            .collect();

        group.bench_function(format!("{}/format_value_sync", scenario.name), |b| {
            b.iter(|| {
                let (env, reg) = fetcher.get_registry_and_environment(&scenario);
                let mut errors = vec![];

                let loc = Localization::with_env(res_ids.clone(), true, env.clone(), reg.clone());
                let bundles = loc.bundles();

                for key in l10n_keys.iter() {
                    bundles.format_value_sync(&key.0, key.1.as_ref(), &mut errors);
                }
            })
        });

        let keys: Vec<L10nKey> = l10n_keys
            .into_iter()
            .map(|key| L10nKey {
                id: key.0.into(),
                args: key.1,
            })
            .collect();
        group.bench_function(format!("{}/format_messages_sync", scenario.name), |b| {
            b.iter(|| {
                let (env, reg) = fetcher.get_registry_and_environment(&scenario);
                let mut errors = vec![];
                let loc = Localization::with_env(res_ids.clone(), true, env.clone(), reg.clone());
                let bundles = loc.bundles();
                bundles.format_messages_sync(&keys, &mut errors);
            })
        });
    }

    group.finish();
}

criterion_group!(benches, preferences_bench);
criterion_main!(benches);
