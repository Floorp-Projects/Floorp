use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;

use futures::stream::Collect;
use futures::stream::FuturesOrdered;
use futures::StreamExt;
use l10nregistry_tests::solver::get_scenarios;
use l10nregistry::solver::{AsyncTester, ParallelProblemSolver, SerialProblemSolver, SyncTester};
use std::future::Future;
use std::pin::Pin;
use std::task::{Context, Poll};

pub struct MockTester {
    values: Vec<Vec<bool>>,
}

impl SyncTester for MockTester {
    fn test_sync(&self, res_idx: usize, source_idx: usize) -> bool {
        self.values[res_idx][source_idx]
    }
}

pub struct SingleTestResult(bool);

impl Future for SingleTestResult {
    type Output = bool;

    fn poll(self: Pin<&mut Self>, _cx: &mut Context<'_>) -> Poll<Self::Output> {
        self.0.into()
    }
}

pub type ResourceSetStream = Collect<FuturesOrdered<SingleTestResult>, Vec<bool>>;
pub struct TestResult(ResourceSetStream);

impl std::marker::Unpin for TestResult {}

impl Future for TestResult {
    type Output = Vec<bool>;

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let pinned = Pin::new(&mut self.0);
        pinned.poll(cx)
    }
}

impl AsyncTester for MockTester {
    type Result = TestResult;

    fn test_async(&self, query: Vec<(usize, usize)>) -> Self::Result {
        let futures = query
            .into_iter()
            .map(|(res_idx, source_idx)| SingleTestResult(self.test_sync(res_idx, source_idx)))
            .collect::<Vec<_>>();
        TestResult(futures.into_iter().collect::<FuturesOrdered<_>>().collect())
    }
}

struct TestStream<'t> {
    solver: ParallelProblemSolver<MockTester>,
    tester: &'t MockTester,
}

impl<'t> TestStream<'t> {
    pub fn new(solver: ParallelProblemSolver<MockTester>, tester: &'t MockTester) -> Self {
        Self { solver, tester }
    }
}

impl<'t> futures::stream::Stream for TestStream<'t> {
    type Item = Vec<usize>;

    fn poll_next(
        mut self: std::pin::Pin<&mut Self>,
        cx: &mut std::task::Context<'_>,
    ) -> std::task::Poll<Option<Self::Item>> {
        let tester = self.tester;
        let solver = &mut self.solver;
        let pinned = std::pin::Pin::new(solver);
        pinned
            .try_poll_next(cx, tester, false)
            .map(|v| v.ok().flatten())
    }
}

fn solver_bench(c: &mut Criterion) {
    let scenarios = get_scenarios();

    let mut group = c.benchmark_group("solver");

    for scenario in scenarios {
        let tester = MockTester {
            values: scenario.values.clone(),
        };

        group.bench_function(&format!("serial/{}", &scenario.name), |b| {
            b.iter(|| {
                let mut gen = SerialProblemSolver::new(scenario.width, scenario.depth);
                while let Ok(Some(_)) = gen.try_next(&tester, false) {}
            })
        });

        {
            let rt = tokio::runtime::Runtime::new().unwrap();

            group.bench_function(&format!("parallel/{}", &scenario.name), |b| {
                b.iter(|| {
                    let gen = ParallelProblemSolver::new(scenario.width, scenario.depth);
                    let mut t = TestStream::new(gen, &tester);
                    rt.block_on(async { while let Some(_) = t.next().await {} });
                })
            });
        }
    }
    group.finish();
}

criterion_group!(benches, solver_bench);
criterion_main!(benches);
