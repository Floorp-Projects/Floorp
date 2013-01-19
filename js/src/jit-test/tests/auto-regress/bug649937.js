// Binary: cache/js-dbg-64-a3eeee8f7803-linux
// Flags: -m -n
//
var i = 0;
var gTestcases = new Array;
var gTc = gTestcases;
function TestCase(n, d, e, a) {
  this.description=d;
  gTestcases[gTc++]=this;
}
function writeTestCaseResult( expect, actual, string ) {};
new TestCase;
test();
function test() {
  for ( gTc=0 ; ; ) {
    i++;
    if (i > 20) { break; }
    gTestcases[gTc].description+=" )";
    gTestcases=[1,];
  }
}
