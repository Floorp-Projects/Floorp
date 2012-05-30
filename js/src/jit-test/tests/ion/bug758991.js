// Don't treat f.caller as a singleton property access, it
// has a non-default getter.
function f(obj) {
    return f.caller;
}
function g(obj) {
    return f(obj);
}
function gg(obj) {
    return f.call(obj, obj);
}

assertEq(g({}), g);

actual = gg(function() {});
assertEq(actual, gg);
