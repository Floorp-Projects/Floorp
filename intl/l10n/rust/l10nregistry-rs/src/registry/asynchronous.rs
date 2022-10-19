use std::{
    pin::Pin,
    task::{Context, Poll},
};

use crate::{
    env::ErrorReporter,
    errors::{L10nRegistryError, L10nRegistrySetupError},
    fluent::{FluentBundle, FluentError},
    registry::{BundleAdapter, L10nRegistry, MetaSources},
    solver::{AsyncTester, ParallelProblemSolver},
    source::{ResourceOption, ResourceStatus},
};

use fluent_fallback::{generator::BundleStream, types::ResourceId};
use futures::{
    stream::{Collect, FuturesOrdered},
    Stream, StreamExt,
};
use std::future::Future;
use unic_langid::LanguageIdentifier;

impl<P, B> L10nRegistry<P, B>
where
    P: Clone,
    B: Clone,
{
    /// This method is useful for testing various configurations.
    #[cfg(feature = "test-fluent")]
    pub fn generate_bundles_for_lang(
        &self,
        langid: LanguageIdentifier,
        resource_ids: Vec<ResourceId>,
    ) -> Result<GenerateBundles<P, B>, L10nRegistrySetupError> {
        let lang_ids = vec![langid];

        Ok(GenerateBundles::new(
            self.clone(),
            lang_ids.into_iter(),
            resource_ids,
            // Cheaply create an immutable shallow copy of the [MetaSources].
            self.try_borrow_metasources()?.clone(),
        ))
    }

    // Asynchronously generate the bundles.
    pub fn generate_bundles(
        &self,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        resource_ids: Vec<ResourceId>,
    ) -> Result<GenerateBundles<P, B>, L10nRegistrySetupError> {
        Ok(GenerateBundles::new(
            self.clone(),
            locales,
            resource_ids,
            // Cheaply create an immutable shallow copy of the [MetaSources].
            self.try_borrow_metasources()?.clone(),
        ))
    }
}

/// This enum contains the various states the [GenerateBundles] can be in during the
/// asynchronous generation step.
enum State<P, B> {
    Empty,
    Locale(LanguageIdentifier),
    Solver {
        locale: LanguageIdentifier,
        solver: ParallelProblemSolver<GenerateBundles<P, B>>,
    },
}

impl<P, B> Default for State<P, B> {
    fn default() -> Self {
        Self::Empty
    }
}

impl<P, B> State<P, B> {
    fn get_locale(&self) -> &LanguageIdentifier {
        match self {
            Self::Locale(locale) => locale,
            Self::Solver { locale, .. } => locale,
            Self::Empty => unreachable!("Attempting to get a locale for an empty state."),
        }
    }

    fn take_solver(&mut self) -> ParallelProblemSolver<GenerateBundles<P, B>> {
        replace_with::replace_with_or_default_and_return(self, |self_| match self_ {
            Self::Solver { locale, solver } => (solver, Self::Locale(locale)),
            _ => unreachable!("Attempting to take a solver in an invalid state."),
        })
    }

    fn put_back_solver(&mut self, solver: ParallelProblemSolver<GenerateBundles<P, B>>) {
        replace_with::replace_with_or_default(self, |self_| match self_ {
            Self::Locale(locale) => Self::Solver { locale, solver },
            _ => unreachable!("Attempting to put back a solver in an invalid state."),
        })
    }
}

pub struct GenerateBundles<P, B> {
    /// Do not access the metasources in the registry, as they may be mutated between
    /// async iterations.
    reg: L10nRegistry<P, B>,
    /// This is an immutable shallow copy of the MetaSources that should not be mutated
    /// during the iteration process. This ensures that the iterator will still be
    /// valid if the L10nRegistry is mutated while iterating through the sources.
    metasources: MetaSources,
    locales: std::vec::IntoIter<LanguageIdentifier>,
    current_metasource: usize,
    resource_ids: Vec<ResourceId>,
    state: State<P, B>,
}

impl<P, B> GenerateBundles<P, B> {
    fn new(
        reg: L10nRegistry<P, B>,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        resource_ids: Vec<ResourceId>,
        metasources: MetaSources,
    ) -> Self {
        Self {
            reg,
            metasources,
            locales,
            current_metasource: 0,
            resource_ids,
            state: State::Empty,
        }
    }
}

pub type ResourceSetStream = Collect<FuturesOrdered<ResourceStatus>, Vec<ResourceOption>>;
pub struct TestResult(ResourceSetStream);
impl std::marker::Unpin for TestResult {}

impl Future for TestResult {
    type Output = Vec<bool>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let pinned = Pin::new(&mut self.0);
        pinned
            .poll(cx)
            .map(|set| set.iter().map(|c| !c.is_required_and_missing()).collect())
    }
}

impl<'l, P, B> AsyncTester for GenerateBundles<P, B> {
    type Result = TestResult;

    fn test_async(&self, query: Vec<(usize, usize)>) -> Self::Result {
        let locale = self.state.get_locale();

        let stream = query
            .iter()
            .map(|(res_idx, source_idx)| {
                let resource_id = &self.resource_ids[*res_idx];
                self.metasources
                    .filesource(self.current_metasource, *source_idx)
                    .fetch_file(locale, resource_id)
            })
            .collect::<FuturesOrdered<_>>();
        TestResult(stream.collect::<_>())
    }
}

#[async_trait::async_trait(?Send)]
impl<P, B> BundleStream for GenerateBundles<P, B> {
    async fn prefetch_async(&mut self) {
        todo!();
    }
}

/// Generate [FluentBundles](FluentBundle) asynchronously.
impl<P, B> Stream for GenerateBundles<P, B>
where
    P: ErrorReporter,
    B: BundleAdapter,
{
    type Item = Result<FluentBundle, (FluentBundle, Vec<FluentError>)>;

    /// Asynchronously try and get a solver, and then with the solver generate a bundle.
    /// If the solver is not ready yet, then this function will return as `Pending`, and
    /// the Future runner will need to re-enter at a later point to try again.
    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        if self.metasources.is_empty() {
            // There are no metasources available, so no bundles can be generated.
            return None.into();
        }
        loop {
            if let State::Solver { .. } = self.state {
                // A solver has already been set up, continue iterating through the
                // resources and generating a bundle.

                // Pin the solver so that the async try_poll_next can be called.
                let mut solver = self.state.take_solver();
                let pinned_solver = Pin::new(&mut solver);

                if let std::task::Poll::Ready(solver_result) =
                    pinned_solver.try_poll_next(cx, &self, false)
                {
                    // The solver is ready, but may not have generated an ordering.

                    if let Ok(Some(order)) = solver_result {
                        // The solver resolved an ordering, and a bundle may be able
                        // to be generated.

                        let bundle = self.metasources.bundle_from_order(
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
                            return bundle.into();
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
                        let solver = ParallelProblemSolver::new(
                            self.resource_ids.len(),
                            self.metasources.get(self.current_metasource).len(),
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

                    // There are no more metasources.
                    self.state = State::Empty;
                    continue;
                }

                // The solver is not ready yet, so exit out of this async task
                // and mark it as pending. It can be tried again later.
                self.state.put_back_solver(solver);
                return std::task::Poll::Pending;
            }

            // There are no more metasources to search.

            // Try the next locale.
            if let Some(locale) = self.locales.next() {
                // Restart at the end of the metasources for this locale, and iterate
                // backwards.
                let last_metasource_idx = self.metasources.len() - 1;
                self.current_metasource = last_metasource_idx;

                let solver = ParallelProblemSolver::new(
                    self.resource_ids.len(),
                    self.metasources.get(self.current_metasource).len(),
                );
                self.state = State::Solver { locale, solver };

                // Continue iterating on the next solver.
                continue;
            }

            // There are no more locales or metasources to search. This iterator
            // is done.
            return None.into();
        }
    }
}
