var objectProxy = Proxy.create({});
var functionProxy = Proxy.createFunction({}, function() {}, function() {});

assertEq(Object.prototype.toString.call(objectProxy), '[object Object]');
assertEq(Object.prototype.toString.call(functionProxy), '[object Function]');
assertEq(Function.prototype.toString.call(functionProxy), 'function () {}');
try {
  Function.prototype.toString.call(objectProxy);
  assertEq(true, false);
} catch (e) {
  assertEq(!!/incompatible/.exec(e), true);
}
