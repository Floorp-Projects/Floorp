function A()
{
    this.a = 77;
    this.b = 88;
}

function B()
{
}

B.prototype = new A;

function C()
{
}

C.prototype = new B;

function f() {
    var o = new C;
    var z;
    for (var i = 0; i < 5; ++i) {
	z = o.a;
	assertEq(z, 77);
    }
}

f();