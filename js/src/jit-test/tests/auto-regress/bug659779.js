// Binary: cache/js-dbg-64-8bcb569c9bf9-linux
// Flags: -m -n -a
//

var gTestcases = new Array;
function TestCase(n, d, e, a) {
  this.description=d
  gTestcases[gTc++]=this
}
TestCase.prototype.dump=function () + toPrinted(this.description)
function toPrinted(value) value=value;
function reportCompare (expected, actual, description) {
  new TestCase("unknown-test-name", description, expected, actual)
}
function enterFunc (funcName) {
  try {
    expectCompile = 'No Error'
    var actualCompile;
    reportCompare(expectCompile, actualCompile, ': compile actual')
  } catch(ex) {}
}
gTc=0;
function jsTestDriverEnd() {
  for (var i = 0; i < gTestcases.length; i++) gTestcases[i].dump()
}
enterFunc();
reportCompare(0, 0, 5.123456);
reportCompare(0, 0, this );
jsTestDriverEnd();
try {
  this.__proto__=[]
} catch(ex) {}
jsTestDriverEnd()
