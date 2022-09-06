// |jit-test| skip-if: helperThreadCount() === 0

load(libdir + "asserts.js");

function BigInteger(a, b, c) {}
function montConvert(x) {
    var r = new BigInteger(null);
    return r;
}
var ba = new Array();
a = new BigInteger(ba);
g = montConvert(a);
var lfGlobal = newGlobal({newCompartment: true});
for (lfLocal in this) {
    if (!(lfLocal in lfGlobal)) {
        lfGlobal[lfLocal] = this[lfLocal];
    }
}
lfGlobal.offThreadCompileToStencil(`
  var dbg = new Debugger(g);
  dbg.onEnterFrame = function (frame) {
    var frameThis = frame.this;
  }
`);
var stencil = lfGlobal.finishOffThreadStencil();
lfGlobal.evalStencil(stencil);
assertThrowsInstanceOf(test, ReferenceError);
function test() {
    function check(fun, msg, todo) {
        success = fun();
    }
    check(() => Object.getPrototypeOf(view) == Object.getPrototypeOf(simple));
    typeof this;
}
