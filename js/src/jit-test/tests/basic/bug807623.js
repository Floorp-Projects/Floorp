var objectProxy = new Proxy({}, {});
var functionProxy = new Proxy(function() {}, {});

assertEq(Object.prototype.toString.call(objectProxy), '[object Object]');
assertEq(Object.prototype.toString.call(functionProxy), '[object Function]');
try {
  Function.prototype.toString.call(functionProxy);
  assertEq(true, false);
} catch (e) {
  assertEq(!!/incompatible/.exec(e), true);
}
try {
  Function.prototype.toString.call(objectProxy);
  assertEq(true, false);
} catch (e) {
  assertEq(!!/incompatible/.exec(e), true);
}
