// |jit-test| slow
"use strict";

let g = (function() {
    "use asm";
    function f() {}
    return f;
})();

oomTest(() => "" + g);
