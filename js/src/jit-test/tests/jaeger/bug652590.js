function f() {
    var x = undefined ? 1 : 4294967295;
    print(false || x);
}
f();
