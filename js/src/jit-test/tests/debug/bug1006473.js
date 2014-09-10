// |jit-test| error: ReferenceError

var lfcode = new Array();
lfcode.push("gczeal(4);");
lfcode.push("setJitCompilerOption('ion.warmup.trigger', 30);");
lfcode.push("\
var g = newGlobal();\
g.parent = this;\
g.eval('function f(frame, exc) { f2 = function () { return exc; }; exc = 123; }');\
g.eval('new Debugger(parent).onExceptionUnwind = f;');\
var obj = int8  ('oops');\
");
while (true) {
    var file = lfcode.shift(); if (file == undefined) { break; }
    loadFile(file)
}
function loadFile(lfVarx) {
    evaluate(lfVarx);
}
