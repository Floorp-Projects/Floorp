// Debugger.prototype.findScripts can filter scripts by URL.
var g1 = newGlobal('new-compartment');
var g2 = newGlobal('new-compartment');
var g3 = newGlobal('new-compartment');

// Define some functions whose url will be this test file.
g1.eval('function g1f() {}');
g2.eval('function g2f() {}');

// Define some functions whose url will be a different file.
url2 = scriptdir + "Debugger-findScripts-08-script2";
load(url2);

var dbg = new Debugger(g1, g2, g3);

var g1fw = dbg.addDebuggee(g1.g1f);
var g1gw = dbg.addDebuggee(g1.g1g);
var g2fw = dbg.addDebuggee(g2.g2f);
var g2gw = dbg.addDebuggee(g2.g2g);

// Find the url of this file.
url = g1fw.script.url;

var scripts;

scripts = dbg.findScripts({});
assertEq(scripts.indexOf(g1fw.script) != -1, true);
assertEq(scripts.indexOf(g1gw.script) != -1, true);
assertEq(scripts.indexOf(g2fw.script) != -1, true);
assertEq(scripts.indexOf(g2gw.script) != -1, true);

scripts = dbg.findScripts({url:url});
assertEq(scripts.indexOf(g1fw.script) != -1, true);
assertEq(scripts.indexOf(g1gw.script) != -1, false);
assertEq(scripts.indexOf(g2fw.script) != -1, true);
assertEq(scripts.indexOf(g2gw.script) != -1, false);

scripts = dbg.findScripts({url:url2});
assertEq(scripts.indexOf(g1fw.script) != -1, false);
assertEq(scripts.indexOf(g1gw.script) != -1, true);
assertEq(scripts.indexOf(g2fw.script) != -1, false);
assertEq(scripts.indexOf(g2gw.script) != -1, true);

scripts = dbg.findScripts({url:url, global:g1});
assertEq(scripts.indexOf(g1fw.script) != -1, true);
assertEq(scripts.indexOf(g1gw.script) != -1, false);
assertEq(scripts.indexOf(g2fw.script) != -1, false);
assertEq(scripts.indexOf(g2gw.script) != -1, false);

scripts = dbg.findScripts({url:url2, global:g1});
assertEq(scripts.indexOf(g1fw.script) != -1, false);
assertEq(scripts.indexOf(g1gw.script) != -1, true);
assertEq(scripts.indexOf(g2fw.script) != -1, false);
assertEq(scripts.indexOf(g2gw.script) != -1, false);

scripts = dbg.findScripts({url:url, global:g2});
assertEq(scripts.indexOf(g1fw.script) != -1, false);
assertEq(scripts.indexOf(g1gw.script) != -1, false);
assertEq(scripts.indexOf(g2fw.script) != -1, true);
assertEq(scripts.indexOf(g2gw.script) != -1, false);

scripts = dbg.findScripts({url:url2, global:g2});
assertEq(scripts.indexOf(g1fw.script) != -1, false);
assertEq(scripts.indexOf(g1gw.script) != -1, false);
assertEq(scripts.indexOf(g2fw.script) != -1, false);
assertEq(scripts.indexOf(g2gw.script) != -1, true);

scripts = dbg.findScripts({url:"xlerb"}); // "XLERB"???
assertEq(scripts.indexOf(g1fw.script) != -1, false);
assertEq(scripts.indexOf(g1gw.script) != -1, false);
assertEq(scripts.indexOf(g2fw.script) != -1, false);
assertEq(scripts.indexOf(g2gw.script) != -1, false);

scripts = dbg.findScripts({url:url, global:g3});
assertEq(scripts.indexOf(g1fw.script) != -1, false);
assertEq(scripts.indexOf(g1gw.script) != -1, false);
assertEq(scripts.indexOf(g2fw.script) != -1, false);
assertEq(scripts.indexOf(g2gw.script) != -1, false);
