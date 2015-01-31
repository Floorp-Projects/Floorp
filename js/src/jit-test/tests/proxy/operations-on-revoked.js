load(libdir + 'asserts.js');

var r = Proxy.revocable({}, {});
var r2 = Proxy.revocable(function(){}, {});
r.revoke();
r2.revoke();

var p = r.proxy;
var p2 = r2.proxy;

assertThrowsInstanceOf(() => ({} instanceof p), TypeError);
assertThrowsInstanceOf(() => ({} instanceof p2), TypeError);

assertEq(Object.prototype.toString.call(p), "[object Object]");
assertEq(Object.prototype.toString.call(p2), "[object Function]");

assertThrowsInstanceOf(() => RegExp.prototype.exec.call(p, ""), TypeError);
assertThrowsInstanceOf(() => RegExp.prototype.exec.call(p2, ""), TypeError);
