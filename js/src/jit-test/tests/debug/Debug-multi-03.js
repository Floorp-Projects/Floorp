// Q: But who shall debug the debuggers?  A: jimb

var log = '';

function addDebug(g, id) {
    var debuggerGlobal = newGlobal('new-compartment');
    debuggerGlobal.debuggee = g;
    debuggerGlobal.id = id;
    debuggerGlobal.print = function (s) { log += s; };
    debuggerGlobal.eval(
        'var dbg = new Debug(debuggee);\n' +
        'dbg.hooks = {\n' +
        '    debuggerHandler: function () { print(id); debugger; print(id); }\n' +
        '};\n');
    return debuggerGlobal;
}

var base = newGlobal('new-compartment');
var top = base;
for (var i = 0; i < 8; i++)  // why have 2 debuggers when you can have 8
    top = addDebug(top, i);
base.eval("debugger;");
assertEq(log, '0123456776543210');
