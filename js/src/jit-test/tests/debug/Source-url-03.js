// Source.prototype.url is null for Function.prototype.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

var Fpw = gw.getOwnPropertyDescriptor('Function').value.proto;
assertEq(Fpw.script.source.url, null);



