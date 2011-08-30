var gTestcases = Array;
function TestCase(n, d, e, a) {
    this.description = d
    gTestcases[gTc] = this
}
TestCase.prototype.dump=function ()  +  +  +
          + this.description +  +
               +  + '\n';function printStatus (msg)
function toPrinted(value) {
}
function reportCompare(expected, actual, description) {
    new TestCase("unknown-test-name", description, expected, actual)
}
gTc = 0;;
function jsTestDriverEnd() {
    for (var i = 0; i < gTestcases.length; i++)
    gTestcases[i].dump()
}
var summary = 'Do not assert with try/finally inside finally';
var expect = 'No Crash';
reportCompare(expect, printStatus, summary);
jsTestDriverEnd();
jsTestDriverEnd();
try {
    f
} catch (ex) {
    actual = ''
}
reportCompare(expect, actual, 5);
jsTestDriverEnd()
