// Binary: cache/js-dbg-64-e8de64e7e9fe-linux
// Flags: --ion-eager
//
function printStatus (msg) {}
function printBugNumber (num) {}
function reportCompare (expected, actual, description) {
    printStatus ("Expected value '" + toPrinted(expected) +  "' matched actual value '" + toPrinted(actual) + "'");
}
try  {
  reportCompare(expectCompile, actualCompile,  summary + ': compile actual');
}   catch(ex)  {  }
var lfcode = new Array();
lfcode.push("\
var bar = {\
    b: 2,\
};\
var results = [];\
for each (let [key, value] in Iterator(bar))\
    results.push(key + \":\" + (results(isXMLName(), \"ok\")));\
var expect = \"a:1;b:2\";\
");
lfcode.push("\
var BUGNUMBER = 244619;\
var summary = 'Don\\'t Crash';\
var actual = 'Crash';\
function f1()\
  eval.call((enterFunc ('test')), \"var a = 'vodka'\");\
gczeal(4);\
reportCompare(expect, actual, summary);\
");
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
			switch (lfRunTypeId) {			}
		} else {
			evaluate(lfVarx);
		}
	} catch (lfVare) {
	}
}
