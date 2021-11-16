// Prevent GC from cancelling/discarding Ion compilations.
gczeal(0);

// This function is used to force a bailout when it is inlined, and to recover
// the frame which is inlining this function.
var resumeHere = function (i) { if (i >= 99) bailout(); };

// This function is used to cause an invalidation after having removed a branch
// after DCE. This is made to check if we correctly recover an array
// allocation.
var uceFault = function (i) {
  if (i > 98)
    uceFault = function (i) { return true; };
  return false;
};

// Check that we correctly allocate the array after taking the recover path.
var uceFault_recoverLength = eval(`(${uceFault})`.replace('uceFault', 'uceFault_recoverLength'));
function recoverLength(i, ...rest) {
  if (uceFault_recoverLength(i) || uceFault_recoverLength(i)) {
    return rest.length;
  }
  assertRecoveredOnBailout(rest, true);
  return 0;
}

var uceFault_recoverContent = eval(`(${uceFault})`.replace('uceFault', 'uceFault_recoverContent'));
function recoverContent(i, ...rest) {
  if (uceFault_recoverContent(i) || uceFault_recoverContent(i)) {
    return rest[0];
  }
  assertRecoveredOnBailout(rest, true);
  return 0;
}

function empty() {}

var uceFault_recoverApply = eval(`(${uceFault})`.replace('uceFault', 'uceFault_recoverApply'));
function recoverApply(i, ...rest) {
  if (uceFault_recoverApply(i) || uceFault_recoverApply(i)) {
    return empty.apply(null, rest);
  }
  assertRecoveredOnBailout(rest, true);
  return 0;
}

var uceFault_recoverSpread = eval(`(${uceFault})`.replace('uceFault', 'uceFault_recoverSpread'));
function recoverSpread(i, ...rest) {
  if (uceFault_recoverSpread(i) || uceFault_recoverSpread(i)) {
    return empty(...rest);
  }
  assertRecoveredOnBailout(rest, true);
  return 0;
}

var uceFault_recoverNewSpread = eval(`(${uceFault})`.replace('uceFault', 'uceFault_recoverNewSpread'));
function recoverNewSpread(i, ...rest) {
  if (uceFault_recoverNewSpread(i) || uceFault_recoverNewSpread(i)) {
    return new empty(...rest);
  }
  assertRecoveredOnBailout(rest, true);
  return 0;
}

var uceFault_recoverSetArgs = eval(`(${uceFault})`.replace('uceFault', 'uceFault_recoverSetArgs'));
function recoverSetArgs(i, ...rest) {
  arguments[1] = "fail";
  if (uceFault_recoverSetArgs(i) || uceFault_recoverSetArgs(i)) {
    // Ensure arguments[1] isn't mapped to rest[0].
    assertEq(rest[0], "ok");
    return 0;
  }
  assertRecoveredOnBailout(rest, true);
  return 0;
}

// Prevent compilation of the top-level
eval(`(${resumeHere})`);

for (let i = 0; i < 100; i++) {
  recoverLength(i);
  recoverContent(i);
  recoverApply(i);
  recoverSpread(i);
  recoverNewSpread(i);
  recoverSetArgs(i, "ok");
}
