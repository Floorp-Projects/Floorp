// |jit-test| --enable-weak-refs
//
// https://tc39.es/proposal-weakrefs/#sec-keepduringjob
// When the abstract operation KeepDuringJob is called with a target object
// reference, it adds the target to an identity Set that will point strongly at
// the target until the end of the current Job.
//
// https://tc39.es/proposal-weakrefs/#sec-weakref-invariants
// When WeakRef.prototype.deref is called, the referent (if it's not already
// dead) is kept alive so that subsequent, synchronous accesses also return the
// object.

let wr;
{
  let obj = {};
  wr = new WeakRef(obj);
  obj = null;
}
// obj is out of block scope now, should be GCed.

gc();

assertEq(undefined == wr.deref(), false);

// test for target is in another zone.
var g = newGlobal({newCompartment: true});
var wr2 = new g.WeakRef({});
assertEq(undefined == wr2.deref(), false);

let wr3 = g.eval("new WeakRef({})");
let wr4 = new WeakRef(evalcx('({})', newGlobal({newCompartment: true})));
let wr5 = new g.WeakRef(evalcx('({})', newGlobal({newCompartment: true})));

var that = this;
(function(){
  let wr6 = new g.WeakRef(newGlobal({newCompartment: true}));
  let wr7 = new WeakRef(wr6);
  let wr8 = new WeakRef(that);
  wr6 = null;
  wr7 = null;
  wr8 = null;
})();

gc();

let wr1;
(function() {
  let obj = {};
  wr1 = new WeakRef(obj);
  obj = null;
})();

// TODO:
// Bug 1603330: WeakRef.deref() makes the target being kept even
// ClearKeptObjects() is called.
//
// don't call wr1.deref() here to prevent the target of wr1 is kept.

gc();

// See https://tc39.es/proposal-weakrefs/#sec-weakref-invariants
// This list is reset when synchronous work is done using the
// ClearKeptObjects abstract operation.
//
// TODO:
// Bug 1603070: The shell has no way to start a new task.
//
// In shell, there's no window object hence there isn't a setTimeout nor event
// handler, so we use this function to simulate that.
// The timeout function in shell doesn't launch a new job, and it will cause
// this test file timeout so I didn't use it here.
function simulateEndOfJob(fn) {
  clearKeptObjects();
  gc();
  fn();
}

simulateEndOfJob(() => {
  assertEq(undefined, wr1.deref());
});

