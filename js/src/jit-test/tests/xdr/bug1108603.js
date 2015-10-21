var caught = false;
try {
  evaluate(cacheEntry(""), {saveBytecode: {value: true}, global: this});
  [[0]];
} catch (err) {
  caught = true;
  assertEq(err.message, "compartment cannot save singleton anymore.");
}
assertEq(caught, true);