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

var target = gDO.getOwnPropertyDescriptor('target').value;
var handler = gDO.getOwnPropertyDescriptor('handler').value;
var proxy = gDO.getOwnPropertyDescriptor('proxy').value;
var proxyProxy = gDO.getOwnPropertyDescriptor('proxyProxy').value;

assertEq(target.isProxy, false);
assertEq(handler.isProxy, false);
assertEq(proxy.isProxy, true);
assertEq(proxyProxy.isProxy, true);

assertEq(target.proxyTarget, undefined);
assertEq(handler.proxyTarget, undefined);
assertEq(proxy.proxyTarget, target);
assertEq(proxyProxy.proxyTarget, proxy);

assertEq(target.proxyHandler, undefined);
assertEq(handler.proxyHandler, undefined);
assertEq(proxy.proxyHandler, handler);
assertEq(proxyProxy.proxyTarget, proxy);
