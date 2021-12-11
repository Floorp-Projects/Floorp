// Test combinations of arguments in different compartments.

gczeal(0);

let heldValues = [];

function ccwToObject() {
    return evalcx('({})', newGlobal({newCompartment: true}));
}

function newRegistry() {
  return new FinalizationRegistry(value => {
    heldValues.push(value);
  });
}

function ccwToRegistry() {
  let global = newGlobal({newCompartment: true});
  global.heldValues = heldValues;
  return global.eval(
    `new FinalizationRegistry(value => heldValues.push(value))`);
}

function incrementalGC() {
  startgc(1);
  while (gcstate() !== "NotActive") {
    gcslice(1000);
  }
}

// Test the case when the registry remains live.
for (let w of [false, true]) {
  for (let x of [false, true]) {
    for (let y of [false, true]) {
      for (let z of [false, true]) {
        let registry = w ? ccwToRegistry(w) : newRegistry();
        let target = x ? ccwToObject() : {};
        let heldValue = y ? ccwToObject() : {};
        let token = z ? ccwToObject() : {};
        registry.register(target, heldValue, token);
        registry.unregister(token);
        registry.register(target, heldValue, token);
        target = undefined;
        token = undefined;
        heldValue = undefined;
        incrementalGC();
        heldValues.length = 0; // Clear, don't replace.
        drainJobQueue();
        assertEq(heldValues.length, 1);
      }
    }
  }
}

// Test the case when registry has no more references.
for (let w of [false, true]) {
  for (let x of [false, true]) {
    for (let y of [false, true]) {
      for (let z of [false, true]) {
        let registry = w ? ccwToRegistry(w) : newRegistry();
        let target = x ? ccwToObject() : {};
        let heldValue = y ? ccwToObject() : {};
        let token = z ? ccwToObject() : {};
        registry.register(target, heldValue, token);
        registry.unregister(token);
        registry.register(target, heldValue, token);
        target = undefined;
        token = undefined;
        heldValue = undefined;
        registry = undefined; // Remove last reference to registry.
        incrementalGC();
        heldValues.length = 0;
        drainJobQueue();
        // The cleanup callback may or may not be run depending on
        // which order the zones are swept in, which itself depends on
        // the arrangement of CCWs.
        assertEq(heldValues.length <= 1, true);
      }
    }
  }
}
