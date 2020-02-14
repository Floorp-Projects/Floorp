// |jit-test| --enable-weak-refs

// Test combinations of arguments in different compartments.

gczeal(0);

let heldValues = [];
let group = new FinalizationGroup(iterator => {
  heldValues.push(...iterator);
});

function ccwToObject() {
    return evalcx('({})', newGlobal({newCompartment: true}));
}

function ccwToGroup() {
  let global = newGlobal({newCompartment: true});
  global.heldValues = heldValues;
  return global.eval(
    `new FinalizationGroup(iterator => heldValues.push(...iterator))`);
}

function incrementalGC() {
  startgc(1);
  while (gcstate() !== "NotActive") {
    gcslice(1000);
  }
}

for (let w of [false, true]) {
  for (let x of [false, true]) {
    for (let y of [false, true]) {
      for (let z of [false, true]) {
        let g = w ? ccwToGroup(w) : group;
        let target = x ? ccwToObject() : {};
        let heldValue = y ? ccwToObject() : {};
        let token = z ? ccwToObject() : {};
        g.register(target, heldValue, token);
        g.unregister(token);
        g.register(target, heldValue, token);
        target = undefined;
        incrementalGC();
        heldValues.length = 0; // Clear, don't replace.
        g.cleanupSome();
        assertEq(heldValues.length, 1);
        assertEq(heldValues[0], heldValue);
      }
    }
  }
}
