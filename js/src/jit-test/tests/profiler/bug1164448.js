// |jit-test| error: TypeError

print = function(s) { return s.toString(); }
var gTestcases = new Array();
function TestCase(n, d, e, a)
  gTestcases[gTc++] = this;
    dump = print;
  for ( gTc=0; gTc < gTestcases.length; gTc++ ) {}
function jsTestDriverEnd() {
  for (var i = 0; i < gTestcases.length; i++)
    gTestcases[i].dump();
}
TestCase();
var g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () {};");
enableSPSProfiling();
if (getBuildConfiguration()["arm-simulator"])
    enableSingleStepProfiling(1);
loadFile("jsTestDriverEnd();");
loadFile("jsTestDriverEnd();");
jsTestDriverEnd();
function loadFile(lfVarx) {
    try { evaluate(lfVarx); } catch (lfVare) {}
}
