use fluent_bundle::FluentArgs;
use fluent_fallback::Localization;
use fluent_testing::get_scenarios;
use l10nregistry::fluent::FluentBundle;
use l10nregistry::registry::BundleAdapter;
use l10nregistry_tests::{RegistrySetup, TestFileFetcher};

#[derive(Clone)]
struct ScenarioBundleAdapter {}

impl BundleAdapter for ScenarioBundleAdapter {
    fn adapt_bundle(&self, bundle: &mut FluentBundle) {
        bundle.set_use_isolating(false);
        bundle
            .add_function("PLATFORM", |_positional, _named| "linux".into())
            .expect("Failed to add a function to the bundle.");
    }
}

#[tokio::test]
async fn scenarios_async() {
    use fluent_testing::scenarios::structs::Scenario;
    let fetcher = TestFileFetcher::new();

    let scenarios = get_scenarios();

    let adapter = ScenarioBundleAdapter {};

    let cannot_produce_bundle = |scenario: &Scenario| {
        scenario
            .queries
            .iter()
            .any(|query| query.exceptional_context.blocks_bundle_generation())
    };

    for scenario in scenarios {
        println!("scenario: {}", scenario.name);
        let setup: RegistrySetup = (&scenario).into();
        let (env, reg) = fetcher.get_registry_and_environment_with_adapter(setup, adapter.clone());

        let loc = Localization::with_env(scenario.res_ids.clone(), false, env.clone(), reg);
        let bundles = loc.bundles();
        let no_bundles = cannot_produce_bundle(&scenario);

        let mut errors = vec![];

        for query in scenario.queries.iter() {
            let errors_start_len = errors.len();

            let args = query.input.args.as_ref().map(|args| {
                let mut result = FluentArgs::new();
                for arg in args.as_slice() {
                    result.set(arg.id.clone(), arg.value.clone());
                }
                result
            });

            if let Some(output) = &query.output {
                if let Some(value) = &output.value {
                    let v = bundles
                        .format_value(&query.input.id, args.as_ref(), &mut errors)
                        .await;
                    if no_bundles || query.exceptional_context.causes_failed_value_lookup() {
                        assert!(v.is_none());
                        if no_bundles {
                            continue;
                        }
                    } else {
                        assert_eq!(v.unwrap(), value.as_str())
                    }
                }
            }

            if query.exceptional_context.causes_reported_format_error() {
                assert!(
                    errors.len() > errors_start_len,
                    "expected reported errors for query {:#?}",
                    query
                );
            } else {
                assert_eq!(
                    errors.len(),
                    errors_start_len,
                    "expected no reported errors for query {:#?}",
                    query
                );
            }
        }

        if scenario
            .queries
            .iter()
            .any(|query| query.exceptional_context.missing_required_resource())
        {
            assert!(
                !env.errors().is_empty(),
                "expected errors for scenario {{ {} }}, but found none",
                scenario.name
            );
        } else {
            assert!(
                env.errors().is_empty(),
                "expected no errors for scenario {{ {} }}, but found {:#?}",
                scenario.name,
                env.errors()
            );
        }
    }
}
