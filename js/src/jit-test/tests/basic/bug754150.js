// |jit-test| error: TypeError;

// String.prototype.replace takes too long time with gczeal(4) if
// --no-baseline --no-ion options.
if (typeof inJit == "function" && typeof inJit() == "string") {
  assertEq(inJit(), "Baseline is disabled.");
  // This test expects TypeError.
  toPrinted(null);
}

function printStatus (msg) {}
function toPrinted(value) {
  value = value.replace(/\\n/g, 'NL')
}
function reportCompare (expected, actual, description) {
    printStatus ("Expected value '" + toPrinted(expected) +  "' matched actual value '" + toPrinted(actual) + "'");
}
var UBound = 0;
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
testThis('x()');
testThis('"abc"()');
testThis('x()');
testThis('Date(12345)()');
testThis('x()');
testThis('1()');
testThis('x()');
testThis('void(0)()');
testThis('x()');
testThis('[1,2,3,4,5](1)');
gczeal(4);
testThis('x(1)');
checkThis('(function (y) {return y+1;})("abc")');
checkThis('f("abc")');
function testThis(sInvalidSyntax) {
  expectedvalues[UBound] = expect;
  actualvalues[UBound] = actual;
  UBound++;
}
function checkThis(sValidSyntax) {
  for (var i=0; i<UBound; i++)
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
}
var actualvalues = [];
for (var i=0; i<UBound; i++)
  reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
