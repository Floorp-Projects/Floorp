// Debugger.Object.prototype.isProxy recognizes (scripted) proxies.
// Debugger.Object.prototype.proxyTarget exposes the [[Proxytarget]] of a proxy.
// Debugger.Object.prototype.proxyHandler exposes the [[ProxyHandler]] of a proxy.

var g = newGlobal();
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);

g.eval('var target = [1,2,3];');
g.eval('var handler = {has: ()=>false};');
g.eval('var proxy = new Proxy(target, handler);');
g.eval('var proxyProxy = new Proxy(proxy, proxy);');
g.eval('var revoker = Proxy.revocable(target, handler);');
g.eval('var revocable = revoker.proxy;');

var target = gDO.getOwnPropertyDescriptor('target').value;
var handler = gDO.getOwnPropertyDescriptor('handler').value;
var proxy = gDO.getOwnPropertyDescriptor('proxy').value;
var proxyProxy = gDO.getOwnPropertyDescriptor('proxyProxy').value;
var revocable = gDO.getOwnPropertyDescriptor('revocable').value;

assertEq(target.isProxy, false);
assertEq(target.proxyTarget, undefined);
assertEq(target.proxyHandler, undefined);

assertEq(handler.isProxy, false);
assertEq(handler.proxyTarget, undefined);
assertEq(handler.proxyHandler, undefined);

assertEq(proxy.isProxy, true);
assertEq(proxy.proxyTarget, target);
assertEq(proxy.proxyHandler, handler);

assertEq(proxyProxy.isProxy, true);
assertEq(proxyProxy.proxyTarget, proxy);
assertEq(proxyProxy.proxyHandler, proxy);

assertEq(revocable.isProxy, true);
assertEq(revocable.proxyTarget, target);
assertEq(revocable.proxyHandler, handler);
g.eval('revoker.revoke();');
assertEq(revocable.isProxy, true);
assertEq(revocable.proxyTarget, null);
assertEq(revocable.proxyHandler, null);
