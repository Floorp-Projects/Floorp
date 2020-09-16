class A {}

class B extends A {
    constructor() {
        super();
    }
}

function h() {}
h = h.bind();

function f() {
    for (var i = 0; i < 1000; ++i) {
        var o = Reflect.construct(B, [], h);
    }
}

for (var i = 0; i < 5; ++i) {
    f();
}
