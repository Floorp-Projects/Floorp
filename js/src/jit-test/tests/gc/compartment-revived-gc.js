// Test 'compartment revived' GCs, where we do an extra GC if there are
// compartments which we expected to die but were kept alive.

// A global used as the destination for transplants.
let transplantTargetGlobal = newGlobal();

function didCompartmentRevivedGC() {
  return performance.mozMemory.gc.lastStartReason === "COMPARTMENT_REVIVED";
}

function compartmentCount() {
  let r = performance.mozMemory.gc.compartmentCount;
  return r;
}

function startIncrementalGC() {
  startgc(1);
  while (gcstate() === "Prepare") {
    gcslice(100, {dontStart: true});
  }
  assertEq(gcstate(), "Mark");
}

function finishIncrementalGC() {
  while (gcstate() !== "NotActive") {
    gcslice(100, {dontStart: true});
  }
  assertEq(gcstate(), "NotActive");
}

// Create a new compartment and global and return the global.
function createCompartment() {
  return newGlobal({newCompartment: true});
}

// Create a transplantable object and create a wrapper to it from a new
// compartment. Return a function to transplant the target object.
function createTransplantableWrapperTarget(wrapperGlobal) {
  let {object: target, transplant} = transplantableObject();
  wrapperGlobal.wrapper = target;
  return transplant;
}

// Transplant an object to a new global by calling the transplant
// function. This remaps all wrappers pointing to the target object,
// potentially keeping dead compartments alive.
function transplantTargetAndRemapWrappers(transplant) {
  transplant(transplantTargetGlobal);
}

// Test no compartment revived GC triggered in normal cases.
function testNormal() {
  gc();
  assertEq(didCompartmentRevivedGC(), false);

  startIncrementalGC();
  finishIncrementalGC();
  assertEq(didCompartmentRevivedGC(), false);

  let initialCount = compartmentCount();
  createCompartment();
  startIncrementalGC();
  finishIncrementalGC();
  assertEq(compartmentCount(), initialCount);
}

// Test compartment revived GC is triggered by wrapper remapping.
function testCompartmentRevived1() {
  let initialCount = compartmentCount();
  let compartment = createCompartment();
  let transplant = createTransplantableWrapperTarget(compartment);
  compartment = null;

  startIncrementalGC();
  transplantTargetAndRemapWrappers(transplant);
  finishIncrementalGC();

  assertEq(didCompartmentRevivedGC(), true);
  assertEq(compartmentCount(), initialCount);
}

// Test no compartment revived GC is triggered for compartments transitively
// kept alive by black roots.
function testCompartmentRevived2() {
  let initialCount = compartmentCount();
  let compartment = createCompartment();
  let transplant = createTransplantableWrapperTarget(compartment);
  let liveCompartment = createCompartment();
  liveCompartment.wrapper = compartment;
  compartment = null;

  startIncrementalGC();
  transplantTargetAndRemapWrappers(transplant);
  finishIncrementalGC();

  assertEq(didCompartmentRevivedGC(), false);
  assertEq(compartmentCount(), initialCount + 2);

  liveCompartment = null;
  gc();

  assertEq(compartmentCount(), initialCount);
}

// Test no compartment revived GC is triggered for compartments transitively
// kept alive by gray roots.
function testCompartmentRevived3() {
  let initialCount = compartmentCount();
  let compartment = createCompartment();
  let transplant = createTransplantableWrapperTarget(compartment);
  let liveCompartment = createCompartment();
  liveCompartment.wrapper = compartment;
  liveCompartment.eval('grayRoot()[0] = this');
  liveCompartment = null;
  gc();

  startIncrementalGC();
  transplantTargetAndRemapWrappers(transplant);
  finishIncrementalGC();

  assertEq(didCompartmentRevivedGC(), false);
  assertEq(compartmentCount(), initialCount + 2);

  // There's no easy way to clear gray roots for a compartment we don't have
  // any reference to.
}

gczeal(0);

testNormal();
testCompartmentRevived1();
testCompartmentRevived2();
testCompartmentRevived3();
