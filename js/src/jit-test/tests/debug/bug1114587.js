// |jit-test| error: TypeError
var lfcode = new Array();
lfcode.push("");
lfcode.push("");
lfcode.push("\
var g = newGlobal();\
g.debuggeeGlobal = this;\
g.eval('(' + function () {\
        dbg = new Debugger(debuggeeGlobal);\
        dbg.onExceptionUnwind = function (frame, exc) {\
            var s = '!';\
            for (var f = frame; f; f = f.older)\
                    s += f.callee.name;\
        };\
    } + ')();');\
Debugger(17) = this;\
");
while (true) {
  var file = lfcode.shift(); if (file == undefined) { break; }
  loadFile(file)
}
function loadFile(lfVarx) {
  if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
    function newFunc(x) { new Function(x)(); }; newFunc(lfVarx); 
  }
}
