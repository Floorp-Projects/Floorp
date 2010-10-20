// |trace-test| error: ReferenceError;

function f() {
    "use strict";
    foo = 1;
}

f();
