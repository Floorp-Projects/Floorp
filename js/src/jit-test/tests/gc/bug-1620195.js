var g99 = newGlobal({});
nukeAllCCWs();
let group = new FinalizationRegistry(x90 => 0);
let other = newGlobal({
    newCompartment: true
});

try {
  group.register(evalcx('({})', other), 1);
} catch (e) {
  // With the args --more-compartments evalcx will throw.
  assertEq(e.message == "can't access dead object" ||
           e.message == "Invalid scope argument to evalcx",
           true);
}
