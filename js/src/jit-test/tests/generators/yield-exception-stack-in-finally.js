load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true})

// Throw an exception from a different compartment.
var thrower = g.Function(`
  throw 0;
`);

function* throwAndYield() {
  try {
    thrower();
  } finally {
    // The |finally| block is entered with the exception stack on the function
    // stack. The function stack is saved in the generator object when executing
    // |yield|, so the exception stack, which was created in a different
    // compartment, has to be wrapped into the current compartment.
    yield;
  }
}
assertThrowsValue(() => [...throwAndYield()], 0);

function* throwAndNuke() {
  try {
    thrower();
  } finally {
    // The function stack contains the wrapped exception stack when entering
    // the |finally| block. When nuking all CCWs it becomes a dead wrapper.
    nukeAllCCWs();
    yield;
  }
}
assertThrowsValue(() => [...throwAndNuke()], 0);
