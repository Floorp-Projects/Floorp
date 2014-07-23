function f() {
    return __proto__
}
f();
f();
assertEq(!!f(), true);
