// |jit-test| error: ReferenceError
function eq(e, a) {
    passed = (a == e);
}
function f(e, a) {
    fail();
    eq(e, a);
}
try {
    f();
} catch (exc1) {}
eq(.1, .1);
var sym = Symbol("method");
evaluate("f(test, sym, 0)", {isRunOnce:true});
