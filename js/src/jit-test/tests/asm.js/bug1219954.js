"use strict";

if (!('oomTest' in this))
    quit();

let g = (function() {
    "use asm";
    function f() {}
    return f;
})();

oomTest(() => "" + g);
