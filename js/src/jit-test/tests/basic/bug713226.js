// |jit-test| mjitalways;
gczeal(4);
var optionNames = options().split(',');
  for (var i = 0; i < optionNames.length; i++)
    var optionName = optionNames[i];
evaluate("\
function addDebug(g, id) {\
    var debuggerGlobal = newGlobal('new-compartment');\
    debuggerGlobal.debuggee = g;\
    debuggerGlobal.id = id;\
    debuggerGlobal.print = function (s) { (g) += s; };\
    debuggerGlobal.eval('var dbg = new Debugger(debuggee);dbg.onDebuggerStatement = function () { print(id); debugger; };');\
    return debuggerGlobal;\
}\
var base = newGlobal('new-compartment');\
var top = base;\
for (var i = 0; i < 8; i++ )\
    top = addDebug(top, i);\
base.eval('debugger;');\
");
