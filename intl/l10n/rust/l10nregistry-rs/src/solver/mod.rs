mod parallel;
mod serial;

pub use parallel::{AsyncTester, ParallelProblemSolver};
pub use serial::{SerialProblemSolver, SyncTester};

pub struct ProblemSolver {
    width: usize,
    depth: usize,

    cache: Vec<Vec<Option<bool>>>,

    solution: Vec<usize>,
    idx: usize,

    dirty: bool,
}

impl ProblemSolver {
    pub fn new(width: usize, depth: usize) -> Self {
        Self {
            width,
            depth,
            cache: vec![vec![None; depth]; width],

            solution: vec![0; width],
            idx: 0,

            dirty: false,
        }
    }
}

impl ProblemSolver {
    pub fn bail(&mut self) -> bool {
        if self.try_advance_source() {
            true
        } else {
            self.try_backtrack()
        }
    }

    pub fn has_missing_cell(&self) -> Option<usize> {
        for res_idx in 0..self.width {
            if self.cache[res_idx].iter().all(|c| *c == Some(false)) {
                return Some(res_idx);
            }
        }
        None
    }

    fn is_cell_missing(&self, res_idx: usize, source_idx: usize) -> bool {
        if let Some(false) = self.cache[res_idx][source_idx] {
            return true;
        }
        false
    }

    fn is_current_cell_missing(&self) -> bool {
        let res_idx = self.idx;
        let source_idx = self.solution[res_idx];
        let cell = &self.cache[res_idx][source_idx];
        if let Some(false) = cell {
            return true;
        }
        false
    }

    pub fn try_advance_resource(&mut self) -> bool {
        if self.idx >= self.width - 1 {
            false
        } else {
            self.idx += 1;
            while self.is_current_cell_missing() {
                if !self.try_advance_source() {
                    return false;
                }
            }
            true
        }
    }

    pub fn try_advance_source(&mut self) -> bool {
        while self.solution[self.idx] < self.depth - 1 {
            self.solution[self.idx] += 1;
            if !self.is_current_cell_missing() {
                return true;
            }
        }
        false
    }

    pub fn try_backtrack(&mut self) -> bool {
        while self.solution[self.idx] == self.depth - 1 {
            if self.idx == 0 {
                return false;
            }
            self.idx -= 1;
        }
        self.solution[self.idx] += 1;
        self.prune()
    }

    pub fn prune(&mut self) -> bool {
        for i in self.idx + 1..self.width {
            let mut source_idx = 0;
            while self.is_cell_missing(i, source_idx) {
                if source_idx >= self.depth - 1 {
                    return false;
                }
                source_idx += 1;
            }
            self.solution[i] = source_idx;
        }
        true
    }

    pub fn is_complete(&self) -> bool {
        self.idx == self.width - 1
    }
}
