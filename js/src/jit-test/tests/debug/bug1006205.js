var lfcode = new Array();
lfcode.push = loadFile;
lfcode.push("\
var g = newGlobal();\
g.debuggeeGlobal = this;\
g.eval(\"(\" + function () {\
        dbg = new Debugger(debuggeeGlobal);\
    } + \")();\");\
");
lfcode.push("gc();");
lfcode.push("\
var g = newGlobal();\
g.debuggeeGlobal = this;\
g.eval(\"(\" + function () {\
  dbg = new Debugger(debuggeeGlobal);\
} + \")();\");\
");
function loadFile(lfVarx) {
function newFunc(x) { new Function(x)(); }; newFunc(lfVarx);
}
