// jitcode which uses objects of one type might not be invalidated when that
// object changes type, if the object isn't accessed in any way.

function foo(x) {
    return x.f;
}

function bar() {
    with({}){}

    var a = {};
    var b = { f: a };
    foo(b);
    a.__proto__ = null;
    foo(b);
}

bar();
