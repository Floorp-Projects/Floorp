// |jit-test| allow-oom

if (helperThreadCount() == 0 || !('oomAfterAllocations' in this))
    quit();

var lfcode = new Array();
lfcode.push("5");
lfcode.push(`
gczeal(8, 2);
try {
    [new String, y]
} catch (e) {}
oomAfterAllocations(50);
try {
    (5).replace(r, () => {});
} catch (x) {}
`);
while (true) {
  var file = lfcode.shift(); if (file == undefined) { break; }
  loadFile(file)
}
function loadFile(lfVarx) {
    if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
        switch (lfRunTypeId) {
            case 5:
                var lfGlobal = newGlobal();
                for (lfLocal in this) {}
                lfGlobal.offThreadCompileScript(lfVarx);
                lfGlobal.runOffThreadScript();
        }
    } else if (!isNaN(lfVarx)) {
        lfRunTypeId = parseInt(lfVarx);
    }
}
