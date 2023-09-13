// https://tc39.es/ecma262/#sec-addtokeptobjects
// When the abstract operation AddToKeptObjects is called with a target object
// reference, it adds the target to an identity Set that will point strongly at
// the target until the end of the current Job.
//
// https://tc39.es/proposal-weakrefs/#sec-weakref-invariants
// When WeakRef.prototype.deref is called, the referent (if it's not already
// dead) is kept alive so that subsequent, synchronous accesses also return the
// object.

function testSameCompartmentWeakRef(
  targetReachable,
  weakRefReachable) {

  let target = {};

  let weakref = new WeakRef(target);
  assertEq(weakref.deref(), target);

  if (!targetReachable) {
    target = undefined;
  }

  if (!weakRefReachable) {
    weakRef = undefined;
  }

  clearKeptObjects();
  gc();

  if (weakRefReachable) {
    if (targetReachable) {
      assertEq(weakref.deref(), target);
    } else {
      assertEq(weakref.deref(), undefined);
    }
  }
}

let serial = 0;

function testCrossCompartmentWeakRef(
  targetReachable,
  weakRefReachable,
  collectTargetZone,
  collectWeakRefZone,
  sameZone) {

  gc();

  let id = serial++;
  let global = newGlobal(sameZone ? {sameZoneAs: this} : {newCompartment: true});
  global.eval('var target = {};');
  global.target.id = id;

  let weakref = new WeakRef(global.target);
  assertEq(weakref.deref(), global.target);

  if (!targetReachable) {
    global.target = undefined;
  }

  if (!weakRefReachable) {
    weakRef = undefined;
  }

  if (collectTargetZone || collectWeakRefZone) {
    clearKeptObjects();

    if (collectTargetZone) {
      schedulezone(global);
    }
    if (collectWeakRefZone) {
      schedulezone(this);
    }

    // Incremental GC so we use sweep groups. Shrinking GC to test updating
    // pointers.
    startgc(1, 'shrinking');
    while (gcstate() !== 'NotActive') {
      gcslice(1000, {dontStart: true});
    }
  }

  if (!(collectWeakRefZone && !weakRefReachable)) {
    if (collectTargetZone && !targetReachable) {
      assertEq(weakref.deref(), undefined);
    } else if (targetReachable) {
      assertEq(weakref.deref(), global.target);
    } else {
      // Target is not strongly reachable but hasn't been collected yet. We
      // can get it back through deref() but must check it based on properties.
      assertEq(weakref.deref() !== undefined, true);
      assertEq(weakref.deref().id, id);
    }
  }
}

gczeal(0);

for (let targetReachable of [true, false]) {
  for (let weakRefReachable of [true, false]) {
    testSameCompartmentWeakRef(targetReachable, weakRefReachable);
  }
}

for (let targetReachable of [true, false]) {
  for (let weakRefReachable of [true, false]) {
    for (let collectTargetZone of [true, false]) {
      for (let collectWeakRefZone of [true, false]) {
        for (let sameZone of [true, false]) {
          if (sameZone && (collectTargetZone != collectWeakRefZone)) {
            continue;
          }

          testCrossCompartmentWeakRef(targetReachable, weakRefReachable,
                                      collectTargetZone, collectWeakRefZone, sameZone);
        }
      }
    }
  }
}
