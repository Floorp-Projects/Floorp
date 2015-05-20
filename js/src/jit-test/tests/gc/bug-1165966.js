// |jit-test| --fuzzing-safe; --thread-count=2; --no-ggc; allow-unhandlable-oom; allow-oom
if (!("oomAfterAllocations" in this))
    quit();
var lfcode = new Array();
lfcode.push(`
var g = newGlobal();
var N = 4;
for (var i = 0; i < N; i++) {
    var dbg = Debugger(g);
    oomAfterAllocations(10);
}
`);
options = function() {}
var lfRunTypeId = -1;
var file = lfcode.shift();
loadFile(file)
function loadFile(lfVarx) {
    if (lfVarx.substr(-7) != ".js" && lfVarx.length != 2) unescape("x");
    try {
        if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
          evaluate(lfVarx);
        }
    } catch (lfVare) {}
}
