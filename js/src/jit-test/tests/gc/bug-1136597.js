// |jit-test| error:ReferenceError
var evalInFrame = (function (global) {
  var dbgGlobal = newGlobal({newCompartment: true});
  var dbg = new dbgGlobal.Debugger();
  return function evalInFrame(upCount, code) {
    dbg.addDebuggee(global);
  };
})(this);
var gTestcases = new Array();
var gTc = gTestcases.length;
function TestCase() {
  return gTestcases[gTc++] = this;
}
function checkCollation(extensionCoValue, usageValue) {
    var collator = new Intl.Collator(["de-DE"]);
    collator.resolvedOptions().collation;
}
checkCollation(undefined, "sort");
checkCollation();
for ( addpow = 0; addpow < 33; addpow++ ) {
    new TestCase();
}
evalInFrame(0, "i(true)", true);
gc(3, 'shrinking')
eval("gc(); h = g1");
