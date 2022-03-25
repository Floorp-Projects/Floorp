use super::{BundleAdapter, L10nRegistry, L10nRegistryLocked};
use crate::env::ErrorReporter;
use crate::errors::L10nRegistryError;
use crate::fluent::{FluentBundle, FluentError};
use crate::solver::{SerialProblemSolver, SyncTester};
use crate::source::ResourceOption;
use fluent_fallback::{generator::BundleIterator, types::ResourceId};
use unic_langid::LanguageIdentifier;

impl<'a, B> L10nRegistryLocked<'a, B> {
    pub(crate) fn bundle_from_order<P>(
        &self,
        metasource: usize,
        locale: LanguageIdentifier,
        source_order: &[usize],
        resource_ids: &[ResourceId],
        error_reporter: &P,
    ) -> Option<Result<FluentBundle, (FluentBundle, Vec<FluentError>)>>
    where
        P: ErrorReporter,
        B: BundleAdapter,
    {
        let mut bundle = FluentBundle::new(vec![locale.clone()]);

        if let Some(bundle_adapter) = self.bundle_adapter {
            bundle_adapter.adapt_bundle(&mut bundle);
        }

        let mut errors = vec![];

        for (&source_idx, resource_id) in source_order.iter().zip(resource_ids.iter()) {
            let source = self.source_idx(metasource, source_idx);
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
    pub fn generate_bundles_for_lang_sync(
        &self,
        langid: LanguageIdentifier,
        resource_ids: Vec<ResourceId>,
    ) -> GenerateBundlesSync<P, B> {
        let lang_ids = vec![langid];

        GenerateBundlesSync::new(self.clone(), lang_ids.into_iter(), resource_ids)
    }

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
            Self::Empty => unreachable!(),
        }
    }

    fn take_solver(&mut self) -> SerialProblemSolver {
        replace_with::replace_with_or_default_and_return(self, |self_| match self_ {
            Self::Solver { locale, solver } => (solver, Self::Locale(locale)),
            _ => unreachable!(),
        })
    }

    fn put_back_solver(&mut self, solver: SerialProblemSolver) {
        replace_with::replace_with_or_default(self, |self_| match self_ {
            Self::Locale(locale) => Self::Solver { locale, solver },
            _ => unreachable!(),
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
            .lock()
            .source_idx(self.current_metasource, source_idx)
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
                self.reg.lock().metasource_len(self.current_metasource),
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

macro_rules! try_next_metasource {
    ( $self:ident ) => {{
        if $self.current_metasource > 0 {
            $self.current_metasource -= 1;
            let solver = SerialProblemSolver::new(
                $self.resource_ids.len(),
                $self.reg.lock().metasource_len($self.current_metasource),
            );
            $self.state = State::Solver {
                locale: $self.state.get_locale().clone(),
                solver,
            };
            continue;
        }
    }};
}

impl<P, B> Iterator for GenerateBundlesSync<P, B>
where
    P: ErrorReporter,
    B: BundleAdapter,
{
    type Item = Result<FluentBundle, (FluentBundle, Vec<FluentError>)>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let State::Solver { .. } = self.state {
                let mut solver = self.state.take_solver();
                match solver.try_next(self, false) {
                    Ok(Some(order)) => {
                        let locale = self.state.get_locale();
                        let bundle = self.reg.lock().bundle_from_order(
                            self.current_metasource,
                            locale.clone(),
                            &order,
                            &self.resource_ids,
                            &self.reg.shared.provider,
                        );
                        self.state.put_back_solver(solver);
                        if bundle.is_some() {
                            return bundle;
                        } else {
                            continue;
                        }
                    }
                    Ok(None) => {
                        try_next_metasource!(self);
                    }
                    Err(idx) => {
                        try_next_metasource!(self);
                        // Only signal an error if we run out of metasources
                        // to try.
                        self.reg.shared.provider.report_errors(vec![
                            L10nRegistryError::MissingResource {
                                locale: self.state.get_locale().clone(),
                                resource_id: self.resource_ids[idx].clone(),
                            },
                        ]);
                    }
                }
                self.state = State::Empty;
            }

            let locale = self.locales.next()?;
            if self.reg.lock().number_of_metasources() == 0 {
                return None;
            }
            self.current_metasource = self.reg.lock().number_of_metasources() - 1;
            let solver = SerialProblemSolver::new(
                self.resource_ids.len(),
                self.reg.lock().metasource_len(self.current_metasource),
            );
            self.state = State::Solver { locale, solver };
        }
    }
}
