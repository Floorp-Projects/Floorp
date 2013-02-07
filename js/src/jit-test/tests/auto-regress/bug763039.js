// Binary: cache/js-dbg-32-5cfb73435e06-linux
// Flags: --ion-eager
//
var actual = '';
function TestCase(n, d, e, a) {
  this.reason = '';
}
function inSection(x) {}
function reportCompare (expected, actual, description) {
  var testcase = new TestCase("unknown-test-name", description, expected, actual);
  testcase.reason = output;
}
var lfcode = new Array();
lfcode.push("4");
lfcode.push("function START(summary) {\
}\
function TEST(section, expected, actual) {\
    return reportCompare(expected, actual, inSection(section) + SUMMARY);\
}\
var expect = (1);\
TEST(1,1 << this <  assertEq++ < this, actual);\
");
lfcode.push("\
gczeal(4);\
data >>>=  RunSingleBenchmark(data);\
");
lfcode.push("4");
lfcode.push("\
var BUGNUMBER = 345855;\
var summary = 'Blank yield expressions are not syntax errors';\
test();\
function test() {\
  try  {\
    eval('(function() {x = 12 + yield;})');\
  }  catch(ex)  {}\
  try  { eval('(function() {x = 12 + yield 42})'); }  catch(ex)  {\
    status = inSection(4);\
  }\
  try  {\
    eval('(function() {x = 12 + (yield);})');\
  }  catch(ex)  {  }\
  try  {\
    eval('(function () {foo((yield))})');\
  }  catch(ex)  {  }\
  try  {\
    eval('(function() {x = 12 + (yield 42)})');\
  }  catch(ex)  {  }\
  reportCompare(expect, actual, summary + ': function() {x = 12 + (yield 42)}');\
}\
");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
                loadFile(file);
}
function loadFile(lfVarx) {
	try {
		if (lfVarx.substr(-3) == ".js") {
		} else if (!isNaN(lfVarx)) {
			lfRunTypeId = lfVarx;
		} else {
			switch (lfRunTypeId) {
				case 1: eval(read(lfVarx)); break;
				default: evaluate(lfVarx);
			}
		}
	} catch (lfVare) {
		print(lfVare);
	}
}
