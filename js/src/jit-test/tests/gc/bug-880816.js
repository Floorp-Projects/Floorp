var lfcode = new Array();
lfcode.push("const baz = 'bar';");
lfcode.push("2");
lfcode.push("{ function foo() {} }");
lfcode.push("evaluate('\
var INVALIDATE_MODES = INVALIDATE_MODE_STRINGS.map(s => ({mode: s}));\
function range(n, m) {}\
function seq_scan(array, f) {}\
function assertStructuralEq(e1, e2) {}\
for (var i = 0, l = a.length; i < l; i++) {}\
');");
lfcode.push("for (var x of Set(Object.getOwnPropertyNames(this))) {}");
var lfRunTypeId = -1;
while (true) {
  var file = lfcode.shift(); if (file == undefined) { break; }
  loadFile(file)
}
function loadFile(lfVarx) {
    try {
        if (lfVarx.substr(-3) == ".js") {}
        if (!isNaN(lfVarx)) {
            lfRunTypeId = parseInt(lfVarx);
        } else {
            switch (lfRunTypeId) {
                case 2: new Function(lfVarx)(); break;
                default: evaluate(lfVarx); break;
            }
       }
    } catch (lfVare) {}
}
