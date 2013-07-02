// Forward to the target if the trap is undefined
var target = function (x, y) {
    this.foo = x + y;
}
var obj = new (Proxy(target, {}))(2, 3);
assertEq(obj.foo, 5);
assertEq(Object.getPrototypeOf(obj), target.prototype);
