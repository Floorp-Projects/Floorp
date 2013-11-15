// |jit-test| error: ReferenceError

var lfcode = new Array();
lfcode.push("3");
lfcode.push("enableSPSProfilingAssertions(false);foo();");
while (true) {
  var file = lfcode.shift(); if (file == undefined) { break; }
  loadFile(file)
}
function loadFile(lfVarx) {
    if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
        switch (lfRunTypeId) {
            default: function newFunc(x) { new Function(x)(); }; newFunc(lfVarx); break;
        }
    } else if (!isNaN(lfVarx)) {
        lfRunTypeId = parseInt(lfVarx);
    }
}
