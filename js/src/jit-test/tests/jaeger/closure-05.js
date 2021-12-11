var gTestcases = new Array();
var gTc = gTestcases.length;
function TestCase(n, d, e, a) {
  gTestcases[gTc++] = this;
}
new TestCase("SECTION", "with MyObject, eval should return square of ");
test();
function test() {
  for (gTc = 0; gTc < gTestcases.length; gTc++) {
    var MYOBJECT = (function isPrototypeOf(message) {
        delete input;
      })();
    with({}) {
      gTestcases[gTc].actual = eval("");
    }
  }
} 
