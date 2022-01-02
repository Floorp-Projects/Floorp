// |jit-test| --no-baseline

assertEq(inJit(), "Baseline is disabled.");
assertEq(inIon(), "Ion is disabled.");
