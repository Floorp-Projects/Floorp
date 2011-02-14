function f(a, b) {
    this.a = a;
    assertEq(b, 'x');
}

for (var x = 0; x < RUNLOOP; ++x) {
    f.prototype = {};
    var obj = new f(x, 'x');
    assertEq(obj.a, x);
    assertEq(Object.getPrototypeOf(obj), f.prototype);
}
