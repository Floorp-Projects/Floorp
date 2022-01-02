function f() {
    return f;
}
f.__proto__ = null;
gc();
f();
new f();
