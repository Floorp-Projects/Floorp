// |jit-test| --fast-warmup

with({}) {}

class A {
    foo() { return 3; }
    get z() { return 5; }
}

class B1 extends A {
    constructor(y) {
        super();
        this.y = y;
    }
}

class B2 extends A {
    constructor(x,y) {
        super();
        this.y = y;
        this.x = x;
    }
}

var sum = 0;
function foo(o) {
    sum += o.foo() + o.y + o.z;
}

for (var i = 0; i < 50; i++) {
    foo(new B1(i));
    foo(new B2(i,i));
}

assertEq(sum, 3250);
