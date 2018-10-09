// |jit-test| slow; skip-if: !('oomTest' in this)
"use strict";

let g = (function() {
    "use asm";
    function f() {}
    return f;
})();

oomTest(() => "" + g);
