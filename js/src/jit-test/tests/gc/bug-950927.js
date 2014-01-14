var lfcode = new Array();
lfcode.push("\
var optionNames = options().split(',');\
  for (var i = 0; i < optionNames.length; i++) {}\
");
lfcode.push("gczeal(7,5);");
lfcode.push("4");
lfcode.push("\
var S = new Array();\
var x = 1;\
for ( var i = 8; i >= 0; i-- ) {\
  S[0] += ' ';\
  S[0] += ',';\
}\
eval(S);\
");
var lfRunTypeId = -1;
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
        loadFile(file)
}
function loadFile(lfVarx) {
        if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
            switch (lfRunTypeId) {
                case 4: eval("(function() { " + lfVarx + " })();"); break;
                default: evaluate(lfVarx, { noScriptRval : true }); break;
            }
        } else if (!isNaN(lfVarx)) {
            lfRunTypeId = parseInt(lfVarx);
    }
}
