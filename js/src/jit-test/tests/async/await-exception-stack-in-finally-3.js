ignoreUnhandledRejections();

var g = newGlobal({newCompartment: true})

// Allow |g| to nuke wrappers from the main compartment.
g.nuke = () => {
  // Nuke wrappers and make sure we don't crash.
  nukeAllCCWs();
};

// Throw an exception from a different compartment.
var thrower = g.Function(`
  nuke();
  throw 0;
`);

async function justThrow() {
  thrower();
}

justThrow();
