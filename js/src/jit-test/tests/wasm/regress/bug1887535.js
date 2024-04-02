// |jit-test| slow;

// Tests the exception handling works during stack overflow.
const v1 = newGlobal({sameZoneAs: this});
class C2 {
    static { }
}

function f() { v1.constructor; }

const { test } = wasmEvalText(`
(module
    (import "" "f" (func $f))
    (export "test" (func $f))
)`, { "": { f, },}).exports;


function f4() {
    try {
       f4();
    } catch(_) {
       test(); test();
    }
}
f4();
