// |jit-test| skip-if: !('oomTest' in this)

newGlobal();
evalcx("oomTest(newGlobal);", newGlobal());
