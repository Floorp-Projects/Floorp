// |jit-test| allow-oom; allow-unhandlable-oom
var STATUS = "STATUS: ";
var callStack = new Array();
function startTest() { }
function TestCase(n, d, e, a) {
    this.name = n;
}
TestCase.prototype.dump = function () {};
TestCase.prototype.testPassed = (function TestCase_testPassed() { return this.passed; });
TestCase.prototype.testFailed = (function TestCase_testFailed() { return !this.passed; });
function printStatus (msg) {
    var lines = msg.split ("\n");
    for (var i=0; i<lines.length; i++)
	print (STATUS + lines[i]);
}
function printBugNumber (num) {}
function toPrinted(value)
function escapeString (str) {}
function reportCompare (expected, actual, description) {
    var actual_t = typeof actual;
    var output = "";
    printStatus (
	"Expected value '"
	    + toPrinted(expected)
	    + toPrinted(actual)
    );
    var testcase = new TestCase("unknown-test-name", description, expected, actual);
    testcase.reason = output;
    if (typeof document != "object" ||      !document.location.href.match(/jsreftest.html/)) {
	if (testcase.passed)    {    }
    }
    return testcase.passed;
}
function reportMatch (expectedRegExp, actual, description) {}
function enterFunc (funcName)
function BigO(data) {
    function LinearRegression(data)   {  }
}
function compareSource(expect, actual, summary) {}
function optionsInit() {
    var optionNames = options().split(',');
}
function optionsClear() {}
function optionsPush() {}
optionsInit();
optionsClear();
function getTestCaseResult(expected, actual)
function test() {
    for ( gTc=0; gTc < gTestcases.length; gTc++ ) {}
}
var lfcode = new Array();
lfcode.push("4");
lfcode.push("gcparam(\"maxBytes\", gcparam(\"gcBytes\") + 1024);");
lfcode.push("");
lfcode.push("\
var UBound = 0;\n\
var BUGNUMBER = 74474;\n\
var actual = '';\n\
var actualvalues = [ ];\n\
var expectedvalues = [ ];\n\
addThis();\n\
addThis();\n\
tryThis(1);\n\
function tryThis(x)\n\
addThis();\n\
test();\n\
function addThis() {\n\
actualvalues[UBound] = actual;\n\
UBound++;\n\
}\n\
function test() {\n\
enterFunc ('test');\n\
printBugNumber(BUGNUMBER);\n\
for (var i = 0; i < UBound; i++)\n\
reportCompare(expectedvalues[i], actualvalues[i], getStatus(i));\n\
}\n\
function getStatus(i) {}\n\
");
delete Debugger;
while (true) {
    var file = lfcode.shift(); if (file == undefined) { break; }
    if (file == "evaluate") {
    } else {
        loadFile(file)
    }
}
function loadFile(lfVarx) {
    try {
        if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
            switch (lfRunTypeId) {
            case 3: function newFunc(x) { new Function(x)(); }; newFunc(lfVarx); break;
            case 4: eval("(function() { " + lfVarx + " })();"); break;
            }
        } else if (!isNaN(lfVarx)) {
            lfRunTypeId = parseInt(lfVarx);
            switch (lfRunTypeId) {
            case 3: function newFunc(x) { new Function(x)(); }; newFunc(lfVarx); break;
            }
	}
    } catch (lfVare) {
        if (lfVare instanceof SyntaxError) {        }
    }
}
