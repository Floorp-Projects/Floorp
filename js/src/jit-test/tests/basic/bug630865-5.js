function C(a, b) {
    this.a = a;
    this.b = b;
}
var f = C.bind(null, 2);
Object.defineProperty(f, "prototype", {get: function () { throw "FAIL"; }});
var x;
for (var i = 0; i < 10; i++)
    x = new f(i);
assertEq(toString.call(x), "[object Object]");
assertEq(Object.getPrototypeOf(x), C.prototype);
assertEq(x.a, 2);
assertEq(x.b, 9);
