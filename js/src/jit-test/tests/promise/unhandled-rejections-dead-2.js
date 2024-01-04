// |jit-test| error:Unhandled rejection

var g = newGlobal({newCompartment: true})
g.outer = this;
g.eval(`
  // Create a new Promise in |outer| using new.target, but with
  // resolver functions in |g|.
  var resolvers;
  var p = Reflect.construct(Promise, [
    (resolve, reject) => {
      resolvers = {resolve, reject};
    }
  ], outer.Promise);

  resolvers.resolve({
    get then() {
      // Throw from the about to be nuked compartment.
      throw null;
    }
  });
`);

// Nuke CCWs, including the SavedFrame for the Promise resolution info.
nukeAllCCWs();
