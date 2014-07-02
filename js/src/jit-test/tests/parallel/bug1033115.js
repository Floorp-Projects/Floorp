// Failed to pass the subreductions as an assignment root to Array.prototype.reducePar,
// fuzzing test case.

if (!getBuildConfiguration().parallelJS)
  quit(0);

x = []
x[8] = ((function() {})());
for each(let a in [0, 0]) {
    x.reducePar(function() {
        return [0];
    });
}
