use super::ProblemSolver;
use std::ops::{Deref, DerefMut};

pub trait SyncTester {
    fn test_sync(&self, res_idx: usize, source_idx: usize) -> bool;
}

pub struct SerialProblemSolver(ProblemSolver);

impl Deref for SerialProblemSolver {
    type Target = ProblemSolver;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl DerefMut for SerialProblemSolver {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.0
    }
}

impl SerialProblemSolver {
    pub fn new(width: usize, depth: usize) -> Self {
        Self(ProblemSolver::new(width, depth))
    }
}

impl SerialProblemSolver {
    fn test_current_cell<T>(&mut self, tester: &T) -> bool
    where
        T: SyncTester,
    {
        let res_idx = self.idx;
        let source_idx = self.solution[res_idx];
        let cell = &mut self.cache[res_idx][source_idx];
        *cell.get_or_insert_with(|| tester.test_sync(res_idx, source_idx))
    }

    pub fn try_next<T>(&mut self, tester: &T, prefetch: bool) -> Result<Option<&[usize]>, usize>
    where
        T: SyncTester,
    {
        if self.width == 0 || self.depth == 0 {
            return Ok(None);
        }
        if self.dirty {
            if !self.bail() {
                return Ok(None);
            }
            self.dirty = false;
        }
        loop {
            if !self.test_current_cell(tester) {
                if !self.bail() {
                    if let Some(res_idx) = self.has_missing_cell() {
                        return Err(res_idx);
                    } else {
                        return Ok(None);
                    }
                }
                continue;
            }
            if self.is_complete() {
                if !prefetch {
                    self.dirty = true;
                }
                return Ok(Some(&self.solution));
            }
            if !self.try_advance_resource() {
                return Ok(None);
            }
        }
    }
}
