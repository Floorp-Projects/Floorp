// |jit-test| allow-oom; skip-if: !('oomTest' in this)

evalcx('oomTest(function() { Array(...""); })', newGlobal());
