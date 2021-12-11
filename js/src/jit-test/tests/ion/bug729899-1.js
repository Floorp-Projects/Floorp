var lfcode = new Array();
lfcode.push("function addThis() {}");
lfcode.push("\
var UBound = 0;\
var expectedvalues = [];\
addThis();\
function addThis() {\
  expectedvalues[UBound] = expect;\
  UBound++;\
}\
");
lfcode.push("\
  var expect = 'No Crash';\
  for (var i = 0; i < (2 << 16); i++) addThis();\
");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
        try { evaluate(file); } catch(lfVare) {}
}
