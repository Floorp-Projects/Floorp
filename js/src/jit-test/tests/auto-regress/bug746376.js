// Binary: cache/js-dbg-64-67bf9a4a1f77-linux
// Flags: --ion-eager
//
var callStack = new Array();
var gTestcases = new Array();
var gTc = gTestcases.length;
function TestCase(n, d, e, a) {
  this.expect = e;
  this.actual = a;
  this.passed = getTestCaseResult(e, a);
  this.reason = '';
  this.bugnumber = typeof(BUGNUMER) != 'undefined' ? BUGNUMBER : '';
  this.type = (typeof window == 'undefined' ? 'shell' : 'browser');
  gTestcases[gTc++] = this;
}
function reportCompare (expected, actual, description) {
  var output = "";
  if (typeof description == "undefined")
  if (expected != actual)
    printStatus ("Expected value '" + toPrinted(expected) +
                 "' matched actual value '" + toPrinted(actual) + "'");
  var testcase = new TestCase("unknown-test-name", description, expected, actual);
  testcase.reason = output;
    if (testcase.passed)     {    }
  return testcase.passed;
}
function enterFunc (funcName) {
  var lastFunc = callStack.pop();
      reportCompare(funcName, lastFunc, "Test driver failure wrong exit function ");
}
function getTestCaseResult(expected, actual) {}
var lfcode = new Array();
lfcode.push("\
var summary = 'decompilation of \"let with with\" ';\
var actual = '';\
var expect = '';\
test();\
function test() {\
  enterFunc ('test');\
  gczeal(2);\
  for (let q = 0; q < 50; ++q) {\
    new Function('for (var i = 0; i < 5; ++i) { } ')();\
    var w = 'r'.match(/r/);\
    new Function('for (var j = 0; j < 1; ++j) { } ')();\
  }\
  reportCompare(expect, actual, summary);\
}\
");
delete Debugger;
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
			evaluate(lfVarx);
		}
	} catch (lfVare) {
	}
}
