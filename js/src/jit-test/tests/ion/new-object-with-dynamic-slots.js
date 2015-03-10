
function Foo() {
    this.a = 1;
    this.b = 1;
    this.c = 1;
    this.d = 1;
    this.e = 1;
    this.f = 1;
    this.g = 1;
    this.h = 1;
    this.i = 1;
    this.j = 1;
    this.k = 1;
    this.l = 1;
    this.m = 1;
    this.n = 1;
    this.o = 1;
    this.p = 1;
    this.q = 1;
    this.r = 1;
    this.s = 1;
}

function fn() {
    var a = [];
    for (var i = 0; i < 50; i++)
        a.push(new Foo());
    var total = 0;
    for (var i = 0; i < a.length; i++) {
        var v = a[i];
        total += v.a + v.b + v.c + v.d + v.e + v.f + v.g + v.h + v.i + v.j + v.k + v.l + v.m + v.n + v.o + v.p + v.q + v.r + v.s;
    }
    assertEq(total, 950);
}
fn();
