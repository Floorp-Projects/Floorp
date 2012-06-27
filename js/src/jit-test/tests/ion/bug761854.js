var gTestcases = new Array();
function TestCase(n, d, e, a) {
  this.name = n;
  this.description = d;
  this.expect = e;
  this.actual = a;
  this.passed = getTestCaseResult(e, a);
  options.stackvalues = [];
function getTestCaseResult(expected, actual) { }
}
var lfcode = new Array();
lfcode.push("3");
lfcode.push("var statusitems = [];\
var actualvalues = [];\
var expectedvalues = [];\
actual = '$a$^'.replace(/\\$\\^/, '--');\
actual = 'ababc'.replace(/abc/, '--');\
actual = 'ababc'.replace(/abc/g, '--');\
");
lfcode.push("\
var SECTION = \"15.4.4.3-1\";\
new TestCase( SECTION, \"Array.prototype.join.length\",           1,      Array.prototype.join.length );\
new TestCase( SECTION, \"delete Array.prototype.join.length\",    false,  delete Array.prototype.join.length );\
new TestCase( SECTION, \"delete Array.prototype.join.length; Array.prototype.join.length\",    1, eval(\"delete Array.prototype.join.length; Array.prototype.join.length\") );\
new TestCase(   SECTION, eval(\"var TEST_ARRAY = new Array(); TEST_ARRAY.join()\") );\
new TestCase(   SECTION, eval(\"var TEST_ARRAY = new Array(); TEST_ARRAY.join(' ')\") );\
new TestCase(   SECTION, eval(\"var TEST_ARRAY = new Array(null, void 0, true, false, 123, new Object(), new Boolean(true) ); TEST_ARRAY.join('&')\") );\
new TestCase(   SECTION, eval(\"var TEST_ARRAY = new Array(null, void 0, true, false, 123, new Object(), new Boolean(true) ); TEST_ARRAY.join('')\") );\
new TestCase(   SECTION, eval(\"var TEST_ARRAY = new Array(null, void 0, true, false, 123, new Object(), new Boolean(true) ); TEST_ARRAY.join(void 0)\") );\
new TestCase(   SECTION, eval(\"var TEST_ARRAY = new Array(null, void 0, true, false, 123, new Object(), new Boolean(true) ); TEST_ARRAY.join()\") );\
new TestCase(   SECTION, eval(\"var TEST_ARRAY = new Array(true); TEST_ARRAY.join('\\v')\") );\
SEPARATOR = \"\\t\";\
new TestCase( SECTION,TEST_ARRAY.join( SEPARATOR ) );\
");
lfcode.push("new TestCase( assertEq,   \"String.prototype.toString()\",        \"\",     String.prototype.toString() );\
new TestCase( SECTION,   \"(new String()).toString()\",          \"\",     (new String()).toString() );\
new TestCase( SECTION,   \"(new String(\\\"\\\")).toString()\",      \"\",     (new String(\"\")).toString() );\
new TestCase( SECTION,   \"(new String( String() )).toString()\",\"\",    (new String(String())).toString() );\
gczeal(4);\
new TestCase( SECTION,   \"(new String( 0 )).toString()\",       \"0\",    (new String((1))).toString() );\
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
				default: evaluate(lfVarx);
			}
		}
	} catch (lfVare) {}
}
