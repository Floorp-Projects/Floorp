
var gTestcases = new Array();
var gTc = gTestcases.length;
function TestCase( a) {
  this.actual = a;
  gTestcases[gTc++] = this;
}
function test() {
  for ( gTc=0; gTc < gTestcases.length; gTc++ ) {
	gTestcases[gTc].actual.toString()
  }
}
function testOverwritingSparseHole() {
  for (var i = 0; i < 50; i++)
    new TestCase(eval("VAR1 = 0; VAR2 = -1; VAR1 %= VAR2; VAR1"));
}
testOverwritingSparseHole();
test();
this.toSource();
