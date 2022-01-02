var lfcode = new Array();
lfcode.push("1");
lfcode.push("");
lfcode.push("0");
lfcode.push("function arguments() { };");
lfcode.push("1");
lfcode.push("\
var GLOBAL_PROPERTIES = new Array();\
var i = 0;\
for ( p in this ) {\
if (p.startsWith('a')) GLOBAL_PROPERTIES[i++] = p;\
}\
for ( i = 0; i < GLOBAL_PROPERTIES.length; i++ ) {\
    eval(GLOBAL_PROPERTIES[i]);\
}\
");
while (true) {
    var file = lfcode.shift(); if (file == undefined) { break; }
    loadFile(file)
}
function loadFile(lfVarx) {
    if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
        switch (lfRunTypeId) {
        case 0: evaluate(lfVarx); break;
        case 1: eval(lfVarx); break;
        }
    } else if (!isNaN(lfVarx)) {
        lfRunTypeId = parseInt(lfVarx);
    }
}
