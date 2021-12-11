var gTestcases = new Array();
var gTc = gTestcases.length;
function TestCase(n, d, e, a) {
    this.passed = getTestCaseResult(e, a);
    gTestcases[gTc++] = this;
}
function getTestCaseResult(expected, actual) {
    if (typeof expected != 'number')
        return actual == expected;
    return Math.abs(actual - expected) <= 1E-10;
}
function test() {
    for ( gTc=0; gTc < gTestcases.length; gTc++ ) {
        gTestcases[gTc].passed = writeTestCaseResult(gTestcases[gTc].description +" = "+ gTestcases[gTc].actual);
    }
    function writeTestCaseResult( expect, actual, string ) {
        var passed = getTestCaseResult( expect, actual );
    }
}
var SECTION = "15.4.2.1-1";
new TestCase( SECTION, eval("var arr = (new Array(1,2)); arr[0]") );
new TestCase( SECTION, "var arr = (new Array(1,2)); String(arr)", "1,2", (this.abstract++));
test();
new TestCase( SECTION, "VAR1 = NaN; VAR2=1; VAR1 -= VAR2", Number.NaN, eval("VAR1 = Number.NaN; VAR2=1; VAR1 -= VAR2"));
