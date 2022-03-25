mod scenarios;

pub use scenarios::get_scenarios;

/// Define a testing scenario.
pub struct Scenario {
    /// Name of the scenario.
    pub name: String,
    /// Number of resources.
    pub width: usize,
    /// Number of sources.
    pub depth: usize,
    /// Vector of resources, containing a vector of sources, with true indicating
    /// whether the resource is present in that source.
    pub values: Vec<Vec<bool>>,
    /// Vector of solutions, each containing a vector of resources, with the index
    /// indicating from which source the resource is chosen.
    /// TODO(issue#17): This field is currently unused!
    pub solutions: Vec<Vec<usize>>,
}

impl Scenario {
    pub fn new<S: ToString>(
        name: S,
        width: usize,
        depth: usize,
        values: Vec<Vec<bool>>,
        solutions: Vec<Vec<usize>>,
    ) -> Self {
        Self {
            name: name.to_string(),
            width,
            depth,
            values,
            solutions,
        }
    }
}
