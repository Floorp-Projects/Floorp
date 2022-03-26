use std::{
    pin::Pin,
    task::{Context, Poll},
};

use super::{BundleAdapter, L10nRegistry, L10nRegistryLocked};
use crate::solver::{AsyncTester, ParallelProblemSolver};
use crate::{
    env::ErrorReporter,
    errors::L10nRegistryError,
    fluent::{FluentBundle, FluentError},
    source::{ResourceOption, ResourceStatus},
};

use fluent_fallback::{generator::BundleStream, types::ResourceId};
use futures::{
    stream::{Collect, FuturesOrdered},
    Stream, StreamExt,
};
use std::future::Future;
use unic_langid::LanguageIdentifier;

impl<'a, B> L10nRegistryLocked<'a, B> {}

impl<P, B> L10nRegistry<P, B>
where
    P: Clone,
    B: Clone,
{
    pub fn generate_bundles_for_lang(
        &self,
        langid: LanguageIdentifier,
        resource_ids: Vec<ResourceId>,
    ) -> GenerateBundles<P, B> {
        let lang_ids = vec![langid];

        GenerateBundles::new(self.clone(), lang_ids.into_iter(), resource_ids)
    }

    pub fn generate_bundles(
        &self,
        locales: std::vec::IntoIter<LanguageIdentifier>,
        resource_ids: Vec<ResourceId>,
    ) -> GenerateBundles<P, B> {
        GenerateBundles::new(self.clone(), locales, resource_ids)
    }
}

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
            Self::Empty => unreachable!(),
        }
    }

    fn take_solver(&mut self) -> ParallelProblemSolver<GenerateBundles<P, B>> {
        replace_with::replace_with_or_default_and_return(self, |self_| match self_ {
            Self::Solver { locale, solver } => (solver, Self::Locale(locale)),
            _ => unreachable!(),
        })
    }

    fn put_back_solver(&mut self, solver: ParallelProblemSolver<GenerateBundles<P, B>>) {
        replace_with::replace_with_or_default(self, |self_| match self_ {
            Self::Locale(locale) => Self::Solver { locale, solver },
            _ => unreachable!(),
        })
    }
}

pub struct GenerateBundles<P, B> {
    reg: L10nRegistry<P, B>,
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
        let lock = self.reg.lock();

        let stream = query
            .iter()
            .map(|(res_idx, source_idx)| {
                let resource_id = &self.resource_ids[*res_idx];
                lock.source_idx(self.current_metasource, *source_idx)
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

macro_rules! try_next_metasource {
    ( $self:ident ) => {{
        if $self.current_metasource > 0 {
            $self.current_metasource -= 1;
            let solver = ParallelProblemSolver::new(
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

impl<P, B> Stream for GenerateBundles<P, B>
where
    P: ErrorReporter,
    B: BundleAdapter,
{
    type Item = Result<FluentBundle, (FluentBundle, Vec<FluentError>)>;

    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        loop {
            if let State::Solver { .. } = self.state {
                let mut solver = self.state.take_solver();
                let pinned_solver = Pin::new(&mut solver);
                match pinned_solver.try_poll_next(cx, &self, false) {
                    std::task::Poll::Ready(order) => match order {
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
                                return bundle.into();
                            } else {
                                continue;
                            }
                        }
                        Ok(None) => {
                            try_next_metasource!(self);
                            self.state = State::Empty;
                            continue;
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
                            self.state = State::Empty;
                            continue;
                        }
                    },
                    std::task::Poll::Pending => {
                        self.state.put_back_solver(solver);
                        return std::task::Poll::Pending;
                    }
                }
            } else if let Some(locale) = self.locales.next() {
                if self.reg.lock().number_of_metasources() == 0 {
                    return None.into();
                }
                let number_of_metasources = self.reg.lock().number_of_metasources() - 1;
                self.current_metasource = number_of_metasources;
                let solver = ParallelProblemSolver::new(
                    self.resource_ids.len(),
                    self.reg.lock().metasource_len(self.current_metasource),
                );
                self.state = State::Solver { locale, solver };
            } else {
                return None.into();
            }
        }
    }
}
