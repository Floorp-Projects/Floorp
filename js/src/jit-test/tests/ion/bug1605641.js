// |jit-test| error:9
function thrower() {
    throw 9;
}
function f() {
    return [...{} [thrower(...["foo"])]] = "undefined";
}
f();
