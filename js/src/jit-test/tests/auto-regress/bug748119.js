// Binary: cache/js-dbg-32-17af008937e3-linux
// Flags: -m -n -a
//
var lfcode = new Array();
lfcode.push("\
var gTestcases = new Array();\
var gTc = gTestcases.length;\
function TestCase(n, d, e, a) {\
  this.passed = getTestCaseResult(e, a);\
  this.bugnumber = typeof(BUGNUMER) != 'undefined' ? BUGNUMBER : '';\
  gTestcases[gTc++] = this;\
}\
for (var i=0; i<len; i++) {}\
function reportCompare (expected, actual, description) {\
  var testcase = new TestCase(\"unknown-test-name\", description, expected, actual);\
  if (testcase.passed)\
  printStatus (\"Expected match to '\" + toPrinted(expectedRegExp) + \"' matched actual value '\" + toPrinted(actual) + \"'\");\
}\
function getTestCaseResult(expected, actual)\
function stopTest() {}\
");
lfcode.push("\
var UBound = 0;\
var TEST_PASSED = 'SyntaxError';\
var TEST_FAILED = 'Generated an error, but NOT a SyntaxError!';\
var statusitems = [];\
var actualvalues = [];\
var expectedvalues = [];\
testThis(' /a**/ ');\
testThis(' /a***/ ');\
testThis(' /a++/ ');\
testThis(' /a+++/ ');\
testThis(' /a???/ ');\
testThis(' /a????/ ');\
testThis(' /+a/ ');\
testThis(' /++a/ ');\
testThis(' /?a/ ');\
testThis(' /??a/ ');\
testThis(' /x{1}{1}/ ');\
testThis(' /x{1,}{1}/ ');\
testThis(' /x{1,2}{1}/ ');\
testThis(' /x{1}{1,}/ ');\
testThis(' /x{1,}{1,}/ ');\
testThis(' /x{1,2}{1,}/ ');\
testThis(' /x{1}{1,2}/ ');\
testThis(' /x{1,}{1,2}/ ');\
function testThis(sInvalidSyntax) {\
  try {\
    eval(sInvalidSyntax);\
  } catch(e) {\
    actual = TEST_PASSED;\
  }\
}\
function checkThis(sAllowedSyntax) {}\
reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);\
exitFunc ('test');\
");
lfcode.push("gczeal(4);");
lfcode.push("\
var MSG_PATTERN = '\\nregexp = ';\
var MSG_STRING = '\\nstring = ';\
var MSG_EXPECT = '\\nExpect: ';\
var MSG_ACTUAL = '\\nActual: ';\
var TYPE_STRING = typeof 'abc';\
function testRegExp(statuses, patterns, strings, actualmatches, expectedmatches)\
        lExpect = expectedmatch.length;\
        lActual = actualmatch.length;\
        var expected = formatArray(expectedmatch);\
          reportCompare(expected, actual, state + ERR_MATCH +  CHAR_NL  );\
function getState(status, pattern, string) {\
  var delim = CHAR_COMMA + CHAR_SPACE;\
");
lfcode.push("\
var gTestcases = new Array;\
function TestCase(n, d, e, a) {}\
function toPrinted(value) value=value.replace(/\\\\n/g, 'NL').replace(/[^\\x20-\\x7E]+/g, escapeString);\
");
lfcode.push("\
var summary = 'Regression test for bug 385393';\
var expect = 'No Crash';\
  reportCompare(expect, actual, summary);\
");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
                loadFile(file);
}
function loadFile(lfVarx) {
	try {
		if (lfVarx.substr(-3) == ".js") {
			switch (lfRunTypeId) {			}
		} else {
			evaluate(lfVarx);
		}
	} catch (lfVare) {	}
}
