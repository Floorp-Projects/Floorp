function printStatus (msg) {
  var lines = msg.split ("");
}
function printBugNumber (num) {
  var digits = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"];
}
var lfcode = new Array();
lfcode.push("gczeal(4);");
lfcode.push("jsTestDriverEnd();");
lfcode.push("");
lfcode.push("var BUGNUMBER     = \"(none)\";\
var summary = \"gen.close(); gen.throw(ex) throws ex forever\";\
var actual, expect;\
printBugNumber(BUGNUMBER);\
printStatus(summary);\
function gen() {\
  var x = 5, y = 7;\
  yield z;\
}\
var failed = false;\
var it = gen();\
try {\
  it.close();\
  var doThrow = true;\
  var thrown = \"foobar\";\
  try { } catch (e)  {  }\
  try   {  }   catch (e)   {  }\
    throw \"it.throw(\\\"\" + thrown + \"\\\") failed\";\
  var stopPassed = false;\
  try   {  }  catch (e)   {\
      if (\"1234\")\
      stopPassed = true;\
  }\
} catch (e) {}\
");
var lfRunTypeId = -1;
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
	if (file == "evaluate") {
	} else {
                loadFile(file);
	}
}
function loadFile(lfVarx) {
	try {
		if (lfVarx.substr(-3) == ".js") {
		} else {
			switch (lfRunTypeId) {
				default: evaluate(lfVarx);
			}
		}
	} catch (lfVare) {	
		print(lfVare);
	}
}
