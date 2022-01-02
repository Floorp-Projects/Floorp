load(libdir + 'asserts.js');

var nativeCode = "function () {\n    [native code]\n}";

var proxy = new Proxy(function() {}, {});
assertEq(Function.prototype.toString.call(proxy), nativeCode);
var o = Proxy.revocable(function() {}, {});
assertEq(Function.prototype.toString.call(o.proxy), nativeCode);
o.revoke();
assertEq(Function.prototype.toString.call(o.proxy), nativeCode);
