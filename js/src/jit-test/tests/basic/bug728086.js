var expected = '';
function TestCase(n, d, e, a) {}
function reportFailure (msg) {}
function toPrinted(value) {
  value = value.replace(/\\\n/g, 'NL')
               .replace(/[^\x20-\x7E]+/g, escapeString);
}
function escapeString (str)   {}
function reportCompare (expected, actual, description) {
  var expected_t = typeof expected;
  var actual_t = typeof actual;
  var output = "";
  var testcase = new TestCase("unknown-test-name", description, expected, actual);
      reportFailure (description + " : " + output);
}
function enterFunc (funcName)
  callStack.push(funcName);
  try  {
    reportCompare(expectCompile, actualCompile,
                  summary + ': compile actual');
  }  catch(ex)  {  }
var lfcode = new Array();
lfcode.push("gczeal(2);\
enterFunc ('test');\
");
lfcode.push("{}");
lfcode.push("noexist.js");
lfcode.push("var BUGNUMBER = 305064;\
var summary = 'Tests the trim, trimRight  and trimLeft methods';\
var trimMethods = ['trim', 'trimLeft', 'trimRight'];\
var whitespace = [\
  {s : '\\u0009', t : 'HORIZONTAL TAB'},\
  {s : '\\u200A', t : 'HAIR SPACE'},\
  ];\
for (var j = 0; j < trimMethods.length; ++j)\
  var method = trimMethods[j];\
    reportCompare(true, true, 'Test skipped. String.prototype.' + method + ' is not supported');\
  str      = '';\
  for (var i = 0; i < whitespace.length; ++i) {\
    var v = whitespace[i].s;\
    var t = (summary)[i].t;\
    v = v + v + v;\
    print('Test ' + method + ' with with leading whitespace. : ' + t);\
    actual = str[method]();\
    reportCompare(expected, actual, t + ':' + '\"' + toPrinted(str) + '\".' + method + '()');\
    str = v + 'a' + v;\
}\
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
			switch (lfRunTypeId) {
			}
		} else {
			eval(lfVarx);
		}
	} catch (lfVare) {	}
}
