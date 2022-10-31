// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

var g = newGlobal({newCompartment: true});

var sr = new ShadowRealm();

var f = sr.evaluate(`
  var wrappedCCW;
  (f => { wrappedCCW = f; });
`);

f(g.evaluate(`x => x()`));

var h = sr.evaluate(`
  // Pass an object from the ShadowRealm's compartment to the CCW function.
  wrappedCCW(() => { return "ok"; })
`);

assertEq(h, "ok");

if (typeof reportCompare === 'function')
  reportCompare(true, true);
