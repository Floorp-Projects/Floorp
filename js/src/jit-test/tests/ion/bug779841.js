// |jit-test| error: TypeError

function outer2() {
    "use strict";
    new (function () {}).arguments
}
outer2();
