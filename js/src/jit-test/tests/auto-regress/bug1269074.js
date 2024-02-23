// |jit-test| allow-oom; skip-if: !hasFunction.oomTest

evalcx('oomTest(function() { Array(...""); })', newGlobal());
