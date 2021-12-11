var lfcode = new Array();
lfcode.push("");
lfcode.push("print('hi');");
while (true) {
        var file = lfcode.shift(); if (file == undefined) { break; }
        loadFile(file);
}
function loadFile(lfVarx) {
        evaluate(lfVarx);
}
