class C { };
C.prototype.a = "a";
C.prototype.b = "b";
C.prototype.c = "c";
C.prototype.d = "d";
C.prototype.e = "e";
C.prototype.f = "f";
C.prototype.g = "g";
C.prototype.h = "h";
C.prototype.i = "i";
C.prototype.j = "j";
C.prototype.k = "k";
C.prototype.l = "l";
C.prototype.m = "m";
C.prototype.n = "n";
C.prototype.o = "o";
C.prototype.p = "p";
C.prototype.q = "q";
C.prototype.r = "r";

class D extends C {
    foo(p) {
        return super[p];
    }
}

var d = new D();

for (let i = 0; i < 20; ++i) {
    for (let p in C.prototype) {
        assertEq(p, d.foo(p));
    }
}
