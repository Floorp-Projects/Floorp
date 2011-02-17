function f(a) {
    this.a = a;
    assertEq(arguments[1], 'x');
}

for (var x = 0; x < RUNLOOP; ++x) {
    f.prototype = {};
    var obj = new f(x, 'x');  // more than f.length arguments
    assertEq(obj.a, x);
    assertEq(Object.getPrototypeOf(obj), f.prototype);
}
