// A scripted proxy can delegate a [[Set]] along to a target
// that's a different kind of proxy.

var target = {};
var wrapper = wrapWithProto(target, null);
var p = new Proxy(wrapper, {});
p.prop = 3;
assertEq(target.prop, 3);
