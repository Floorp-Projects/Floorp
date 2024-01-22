load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true})

// Throw an exception from a different compartment.
var thrower = g.Function(`
  throw 0;
`);

async function throwAndAwait() {
  try {
    thrower();
  } finally {
    // The |finally| block is entered with the exception stack on the function
    // stack. The function stack is saved in the generator object when executing
    // |await|, so the exception stack, which was created in a different
    // compartment, has to be wrapped into the current compartment.
    await {};
  }
}
throwAndAwait().then(() => {
  throw new Error("missing error");
}, e => {
  assertEq(e, 0);
});
