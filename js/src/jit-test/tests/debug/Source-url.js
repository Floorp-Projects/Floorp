// Source.prototype.url can be a string or null.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
for (var fileName of ['file:///var/foo.js', null]) {
    g.evaluate("function f(x) { return 2*x; }", {fileName: fileName});
    var fw = gw.getOwnPropertyDescriptor('f').value;
    assertEq(fw.script.source.url, fileName);
}
