if (!('oomTest' in this))
  quit();

// Adapted from randomly chosen test: js/src/jit-test/tests/profiler/bug1231925.js
"use strict";
enableSPSProfiling();
oomTest(function() {
    eval("(function() {})()");
});
