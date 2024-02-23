// |jit-test| skip-if: !hasFunction.oomTest
newGlobal();
evalcx("oomTest(newGlobal);", newGlobal());
