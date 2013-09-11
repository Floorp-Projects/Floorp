// Debugger.prototype.findScripts can find the innermost script at a given
// source location.
var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

function script(f) {
    return gw.makeDebuggeeValue(f).script;
}

function arrayIsOnly(array, element) {
    return array.length == 1 && array[0] === element;
}

url = scriptdir + 'Debugger-findScripts-14.script1';
g.load(url);

var scripts;

// When we're doing 'innermost' queries, we don't have to worry about finding
// random eval scripts: we should get exactly one script, for the function
// covering that line.
scripts = dbg.findScripts({url:url, line:4, innermost:true});
assertEq(arrayIsOnly(scripts, script(g.f)), true);

scripts = dbg.findScripts({url:url, line:6, innermost:true});
assertEq(arrayIsOnly(scripts, script(g.f())), true);

scripts = dbg.findScripts({url:url, line:8, innermost:true});
assertEq(arrayIsOnly(scripts, script(g.f()())), true);
