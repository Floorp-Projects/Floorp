ignoreUnhandledRejections();

var g = newGlobal({newCompartment: true})

// Throw an exception from a different compartment.
var thrower = g.Function(`
  throw 0;
`);

async function justThrow() {
  thrower();
}

justThrow();

// Nuke wrappers and make sure we don't crash.
nukeAllCCWs();
