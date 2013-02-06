// Binary: cache/js-dbg-32-4bcbb63b89c3-linux
// Flags: --ion-eager
//
var summary = '';
function reportFailure (msg) {}
function toPrinted(value) {
  value = value.replace(/\n/g, 'NL')
}
function reportCompare (expected, actual, description) {
  var output = "";
  output += "Expected value '" + toPrinted(expected) +
      "', Actual value '" + toPrinted(actual) + "' ";
      reportFailure (description + " : " + output);
}
var lfcode = new Array();
lfcode.push("\
  expect = actual = 'No Exception';\
  reportCompare(expect, actual, summary);\
");
lfcode.push("\
function reportFailure (section, msg)\
  msg = inSection(section)+\"\"+msg;\
");
lfcode.push("\
try {\
  for (var i in expect) \
    reportCompare(expect[i], actual[i], getStatus(i));\
} catch(exc1) {}\
function getStatus(i) {}\
");
lfcode.push("gczeal(2,(9));");
lfcode.push("evaluate(\"reportCompare(expect, actual, summary);\");");
	gcPreserveCode()
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
	if (file == "evaluate") {
	} else {
                loadFile(file);
	}
}
function loadFile(lfVarx) {
	try {
		if (lfVarx.substr(-3) != ".js") {
			evaluate(lfVarx);
		}
	} catch (lfVare) {}
}
