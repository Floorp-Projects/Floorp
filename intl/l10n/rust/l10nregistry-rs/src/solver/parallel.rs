use super::ProblemSolver;
use std::ops::{Deref, DerefMut};

use futures::ready;
use std::future::Future;
use std::pin::Pin;

pub trait AsyncTester {
    type Result: Future<Output = Vec<bool>>;

    fn test_async(&self, query: Vec<(usize, usize)>) -> Self::Result;
}

pub struct ParallelProblemSolver<T>
where
    T: AsyncTester,
{
    solver: ProblemSolver,
    current_test: Option<(T::Result, Vec<usize>)>,
}

impl<T: AsyncTester> Deref for ParallelProblemSolver<T> {
    type Target = ProblemSolver;

    fn deref(&self) -> &Self::Target {
        &self.solver
    }
}

impl<T: AsyncTester> DerefMut for ParallelProblemSolver<T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.solver
    }
}

impl<T: AsyncTester> ParallelProblemSolver<T> {
    pub fn new(width: usize, depth: usize) -> Self {
        Self {
            solver: ProblemSolver::new(width, depth),
            current_test: None,
        }
    }
}

type TestQuery = (Vec<(usize, usize)>, Vec<usize>);

impl<T: AsyncTester> ParallelProblemSolver<T> {
    pub fn try_generate_complete_candidate(&mut self) -> bool {
        while !self.is_complete() {
            while self.is_current_cell_missing() {
                if !self.try_advance_source() {
                    return false;
                }
            }
            if !self.try_advance_resource() {
                return false;
            }
        }
        true
    }

    fn try_generate_test_query(&mut self) -> Result<TestQuery, usize> {
        let mut test_cells = vec![];
        let query = self
            .solution
            .iter()
            .enumerate()
            .filter_map(|(res_idx, source_idx)| {
                let cell = self.cache[res_idx][*source_idx];
                match cell {
                    None => {
                        test_cells.push(res_idx);
                        Some(Ok((res_idx, *source_idx)))
                    }
                    Some(false) => Some(Err(res_idx)),
                    Some(true) => None,
                }
            })
            .collect::<Result<_, _>>()?;
        Ok((query, test_cells))
    }

    fn apply_test_result(
        &mut self,
        resources: Vec<bool>,
        testing_cells: Vec<usize>,
    ) -> Result<(), usize> {
        let mut first_missing = None;
        for (result, res_idx) in resources.into_iter().zip(testing_cells) {
            let source_idx = self.solution[res_idx];
            self.cache[res_idx][source_idx] = Some(result);
            if !result && first_missing.is_none() {
                first_missing = Some(res_idx);
            }
        }
        if let Some(idx) = first_missing {
            Err(idx)
        } else {
            Ok(())
        }
    }

    pub fn try_poll_next(
        mut self: std::pin::Pin<&mut Self>,
        cx: &mut std::task::Context<'_>,
        tester: &T,
        prefetch: bool,
    ) -> std::task::Poll<Result<Option<Vec<usize>>, usize>>
    where
        <T as AsyncTester>::Result: Unpin,
    {
        if self.width == 0 || self.depth == 0 {
            return Ok(None).into();
        }

        'outer: loop {
            if let Some((test, testing_cells)) = &mut self.current_test {
                let pinned = Pin::new(test);
                let set = ready!(pinned.poll(cx));
                let testing_cells = testing_cells.clone();

                if let Err(res_idx) = self.apply_test_result(set, testing_cells) {
                    self.idx = res_idx;
                    self.prune();
                    if !self.bail() {
                        if let Some(res_idx) = self.has_missing_cell() {
                            return Err(res_idx).into();
                        } else {
                            return Ok(None).into();
                        }
                    }
                    self.current_test = None;
                    continue 'outer;
                } else {
                    self.current_test = None;
                    if !prefetch {
                        self.dirty = true;
                    }
                    return Ok(Some(self.solution.clone())).into();
                }
            } else {
                if self.dirty {
                    if !self.bail() {
                        if let Some(res_idx) = self.has_missing_cell() {
                            return Err(res_idx).into();
                        } else {
                            return Ok(None).into();
                        }
                    }
                    self.dirty = false;
                }
                while self.try_generate_complete_candidate() {
                    match self.try_generate_test_query() {
                        Ok((query, testing_cells)) => {
                            self.current_test = Some((tester.test_async(query), testing_cells));
                            continue 'outer;
                        }
                        Err(res_idx) => {
                            self.idx = res_idx;
                            self.prune();
                            if !self.bail() {
                                if let Some(res_idx) = self.has_missing_cell() {
                                    return Err(res_idx).into();
                                } else {
                                    return Ok(None).into();
                                }
                            }
                        }
                    }
                }
                return Ok(None).into();
            }
        }
    }
}
