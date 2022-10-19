use super::{BundleAdapter, L10nRegistry, MetaSources};
use crate::env::ErrorReporter;
use crate::errors::L10nRegistryError;
use crate::fluent::{FluentBundle, FluentError};
use crate::solver::{SerialProblemSolver, SyncTester};
use crate::source::ResourceOption;
use fluent_fallback::{generator::BundleIterator, types::ResourceId};
use unic_langid::LanguageIdentifier;

impl MetaSources {
    pub(crate) fn bundle_from_order<P, B>(
        &self,
        metasource: usize,
        locale: LanguageIdentifier,
        source_order: &[usize],
        resource_ids: &[ResourceId],
        error_reporter: &P,
        bundle_adapter: Option<&B>,
    ) -> Option<Result<FluentBundle, (FluentBundle, Vec<FluentError>)>>
    where
        P: ErrorReporter,
        B: BundleAdapter,
    {
        let mut bundle = FluentBundle::new(vec![locale.clone()]);

        if let Some(bundle_adapter) = bundle_adapter {
            bundle_adapter.adapt_bundle(&mut bundle);
        }

        let mut errors = vec![];

        for (&source_idx, resource_id) in source_order.iter().zip(resource_ids.iter()) {
            let source = self.filesource(metasource, source_idx);
            if let ResourceOption::Some(res) =
                source.fetch_file_sync(&locale, resource_id, /* overload */ true)
            {
                if source.options.allow_override {
                    bundle.add_resource_overriding(res);
                } else if let Err(err) = bundle.add_resource(res) {
                    errors.extend(err.into_iter().map(|error| L10nRegistryError::FluentError {
                        resource_id: resource_id.clone(),
                        loc: None,
                        error,
                    }));
                }
            } else if resource_id.is_required() {
                return None;
            }
        }

        if !errors.is_empty() {
            error_reporter.report_errors(errors);
        }
        Some(Ok(bundle))
    }
}

impl<P, B> L10nRegistry<P, B>
where
    P: Clone,
    B: Clone,
{
    /// A test-only function for easily generating bundles for a single langid.
    #[cfg(feature = "test-fluent")]
    pub fn generate_bundles_for_lang_sync(
        &self,
        langid: LanguageIdentifier,
        resource_ids: Vec<ResourceId>,
    ) -> GenerateBundlesSync<P, B> {
        let lang_ids = vec![langid];

        GenerateBundlesSync::new(self.clone(), lang_ids.into_iter(), resource_ids)
    }

    /// Wiring for hooking up the synchronous bundle generation to the
    /// [BundleGenerator] trait.
    pub fn generate_bundles_sync(
        &self,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        resource_ids: Vec<ResourceId>,
    ) -> GenerateBundlesSync<P, B> {
        GenerateBundlesSync::new(self.clone(), locales, resource_ids)
    }
}

enum State {
    Empty,
    Locale(LanguageIdentifier),
    Solver {
        locale: LanguageIdentifier,
        solver: SerialProblemSolver,
    },
}

impl Default for State {
    fn default() -> Self {
        Self::Empty
    }
}

impl State {
    fn get_locale(&self) -> &LanguageIdentifier {
        match self {
            Self::Locale(locale) => locale,
            Self::Solver { locale, .. } => locale,
            Self::Empty => unreachable!("Attempting to get a locale for an empty state."),
        }
    }

    fn take_solver(&mut self) -> SerialProblemSolver {
        replace_with::replace_with_or_default_and_return(self, |self_| match self_ {
            Self::Solver { locale, solver } => (solver, Self::Locale(locale)),
            _ => unreachable!("Attempting to take a solver in an invalid state."),
        })
    }

    fn put_back_solver(&mut self, solver: SerialProblemSolver) {
        replace_with::replace_with_or_default(self, |self_| match self_ {
            Self::Locale(locale) => Self::Solver { locale, solver },
            _ => unreachable!("Attempting to put back a solver in an invalid state."),
        })
    }
}

pub struct GenerateBundlesSync<P, B> {
    reg: L10nRegistry<P, B>,
    locales: std::vec::IntoIter<LanguageIdentifier>,
    current_metasource: usize,
    resource_ids: Vec<ResourceId>,
    state: State,
}

impl<P, B> GenerateBundlesSync<P, B> {
    fn new(
        reg: L10nRegistry<P, B>,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        resource_ids: Vec<ResourceId>,
    ) -> Self {
        Self {
            reg,
            locales,
            current_metasource: 0,
            resource_ids,
            state: State::Empty,
        }
    }
}

impl<P, B> SyncTester for GenerateBundlesSync<P, B> {
    fn test_sync(&self, res_idx: usize, source_idx: usize) -> bool {
        let locale = self.state.get_locale();
        let resource_id = &self.resource_ids[res_idx];
        !self
            .reg
            .try_borrow_metasources()
            .expect("Unable to get the MetaSources.")
            .filesource(self.current_metasource, source_idx)
            .fetch_file_sync(locale, resource_id, /* overload */ true)
            .is_required_and_missing()
    }
}

impl<P, B> BundleIterator for GenerateBundlesSync<P, B>
where
    P: ErrorReporter,
{
    fn prefetch_sync(&mut self) {
        if let State::Solver { .. } = self.state {
            let mut solver = self.state.take_solver();
            if let Err(idx) = solver.try_next(self, true) {
                self.reg
                    .shared
                    .provider
                    .report_errors(vec![L10nRegistryError::MissingResource {
                        locale: self.state.get_locale().clone(),
                        resource_id: self.resource_ids[idx].clone(),
                    }]);
            }
            self.state.put_back_solver(solver);
            return;
        }

        if let Some(locale) = self.locales.next() {
            let mut solver = SerialProblemSolver::new(
                self.resource_ids.len(),
                self.reg
                    .try_borrow_metasources()
                    .expect("Unable to get the MetaSources.")
                    .get(self.current_metasource)
                    .len(),
            );
            self.state = State::Locale(locale.clone());
            if let Err(idx) = solver.try_next(self, true) {
                self.reg
                    .shared
                    .provider
                    .report_errors(vec![L10nRegistryError::MissingResource {
                        locale,
                        resource_id: self.resource_ids[idx].clone(),
                    }]);
            }
            self.state.put_back_solver(solver);
        }
    }
}

impl<P, B> Iterator for GenerateBundlesSync<P, B>
where
    P: ErrorReporter,
    B: BundleAdapter,
{
    type Item = Result<FluentBundle, (FluentBundle, Vec<FluentError>)>;

    /// Synchronously generate a bundle based on a solver.
    fn next(&mut self) -> Option<Self::Item> {
        let metasources = self
            .reg
            .try_borrow_metasources()
            .expect("Unable to get the MetaSources.");

        if metasources.is_empty() {
            // There are no metasources available, so no bundles can be generated.
            return None;
        }

        loop {
            if let State::Solver { .. } = self.state {
                // A solver has already been set up, continue iterating through the
                // resources and generating a bundle.
                let mut solver = self.state.take_solver();
                let solver_result = solver.try_next(self, false);

                if let Ok(Some(order)) = solver_result {
                    // The solver resolved an ordering, and a bundle may be able
                    // to be generated.

                    let bundle = metasources.bundle_from_order(
                        self.current_metasource,
                        self.state.get_locale().clone(),
                        &order,
                        &self.resource_ids,
                        &self.reg.shared.provider,
                        self.reg.shared.bundle_adapter.as_ref(),
                    );

                    self.state.put_back_solver(solver);

                    if bundle.is_some() {
                        // The bundle was successfully generated.
                        return bundle;
                    }

                    // No bundle was generated, continue on.
                    continue;
                }

                // There is no bundle ordering available.

                if self.current_metasource > 0 {
                    // There are more metasources, create a new solver and try the
                    // next metasource. If there is an error in the solver_result
                    // ignore it for now, since there are more metasources.
                    self.current_metasource -= 1;
                    let solver = SerialProblemSolver::new(
                        self.resource_ids.len(),
                        metasources.get(self.current_metasource).len(),
                    );
                    self.state = State::Solver {
                        locale: self.state.get_locale().clone(),
                        solver,
                    };
                    continue;
                }

                if let Err(idx) = solver_result {
                    // Since there are no more metasources, and there is an error,
                    // report it instead of ignoring it.
                    self.reg.shared.provider.report_errors(vec![
                        L10nRegistryError::MissingResource {
                            locale: self.state.get_locale().clone(),
                            resource_id: self.resource_ids[idx].clone(),
                        },
                    ]);
                }

                self.state = State::Empty;
                continue;
            }

            // Try the next locale, or break out of the loop if there are none left.
            let locale = self.locales.next()?;

            // Restart at the end of the metasources for this locale, and iterate
            // backwards.
            let last_metasource_idx = metasources.len() - 1;
            self.current_metasource = last_metasource_idx;

            let solver = SerialProblemSolver::new(
                self.resource_ids.len(),
                metasources.get(self.current_metasource).len(),
            );

            // Continue iterating on the next solver.
            self.state = State::Solver { locale, solver };
        }
    }
}
