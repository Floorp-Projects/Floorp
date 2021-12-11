var lfcode = new Array();
lfcode.push("");
lfcode.push("0");
lfcode.push("newGlobal(\"1\").eval(\"(new Debugger).addAllGlobalsAsDebuggees();\");\n");
while (true) {
    var file = lfcode.shift(); if (file == undefined) { break; }
    loadFile(file)
}
function loadFile(lfVarx) {
    try {
      if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
        evaluate(lfVarx); 
      } else if (!isNaN(lfVarx)) {}
    } catch (lfVare) {    }
}
