// Source.prototype.url returns a synthesized URL for eval code.

var g = newGlobal({newCompartment: true});
g.eval('function double() { return 2*x }');

var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

var fw = gw.getOwnPropertyDescriptor('double').value;
assertEq(!!fw.script.source.url.match(/Source-url-01.js line [0-9]+ > eval/), true);
