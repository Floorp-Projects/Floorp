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

function empty() {}

var globalArgs;
function escape(args) { globalArgs = args; }

// Check rest array length.
function length(i, ...rest) {
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function lengthBail(i, ...rest) {
  resumeHere(i);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

// Check rest array content.
function content(...rest) {
  assertEq(rest[0], rest[1]);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function contentBail(...rest) {
  resumeHere(rest[0]);
  assertEq(rest[0], rest[1]);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function contentExtraFormals(i, ...rest) {
  assertEq(rest[0], i);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}
function contentExtraFormalsBail(i, ...rest) {
  resumeHere(i);
  assertEq(rest[0], i);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

// No scalar replacement when the rest array is modified.
function setLength(i, ...rest) {
  rest.length = 0;
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function setLengthBail(i, ...rest) {
  resumeHere(i);
  rest.length = 0;
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function setContent(i, ...rest) {
  rest[0] = "bad";
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function setContentBail(i, ...rest) {
  resumeHere(i);
  rest[0] = "bad";
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function deleteContent(i, ...rest) {
  delete rest[0];
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function deleteContentBail(i, ...rest) {
  resumeHere(i);
  delete rest[0];
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

// No scalar replacement when the rest array escapes.
function escapes(i, ...rest) {
  escape(rest);
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function escapesBail(i, ...rest) {
  resumeHere(i);
  escape(rest);
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

// Check rest array with Function.prototype.apply.
function apply(...rest) {
  empty.apply(null, rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function applyBail(...rest) {
  resumeHere(rest[0]);
  empty.apply(null, rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function applyExtraFormals(i, ...rest) {
  empty.apply(null, rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function applyExtraFormalsBail(i, ...rest) {
  resumeHere(i);
  empty.apply(null, rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

// Check rest array with spread.
function spread(...rest) {
  empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function spreadBail(...rest) {
  resumeHere(rest[0]);
  empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function spreadExtraFormals(i, ...rest) {
  empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function spreadExtraFormalsBail(i, ...rest) {
  resumeHere(i);
  empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

// Extra args currently not supported.
function spreadExtraArgs(...rest) {
  empty(0, ...rest);
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function spreadExtraArgsBail(...rest) {
  resumeHere(rest[0]);
  empty(0, ...rest);
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

// Check rest array with new spread.
function newSpread(...rest) {
  new empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function newSpreadBail(...rest) {
  resumeHere(rest[0]);
  new empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function newSpreadExtraFormals(i, ...rest) {
  new empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function newSpreadExtraFormalsBail(i, ...rest) {
  resumeHere(i);
  new empty(...rest);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

// Extra args currently not supported.
function newSpreadExtraArgs(...rest) {
  new empty(0, ...rest);
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

function newSpreadExtraArgsBail(...rest) {
  resumeHere(rest[0]);
  new empty(0, ...rest);
  assertRecoveredOnBailout(rest, false);
  return rest.length;
}

// The arguments object isn't mapped.
function setArgs(i, ...rest) {
  arguments[1] = "fail";
  assertEq(rest[0], i);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

function setArgsBail(i, ...rest) {
  resumeHere(i);
  arguments[1] = "fail";
  assertEq(rest[0], i);
  assertRecoveredOnBailout(rest, true);
  return rest.length;
}

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
  length(i);
  lengthBail(i);
  content(i, i);
  contentBail(i, i);
  contentExtraFormals(i, i);
  contentExtraFormalsBail(i, i);
  setLength(i);
  setLengthBail(i);
  setContent(i, i);
  setContentBail(i, i);
  deleteContent(i, i);
  deleteContentBail(i, i);
  escapes(i);
  escapesBail(i);
  apply(i);
  applyBail(i);
  applyExtraFormals(i);
  applyExtraFormalsBail(i);
  spread(i);
  spreadBail(i);
  spreadExtraFormals(i);
  spreadExtraFormalsBail(i);
  spreadExtraArgs(i);
  spreadExtraArgsBail(i);
  newSpread(i);
  newSpreadBail(i);
  newSpreadExtraFormals(i);
  newSpreadExtraFormalsBail(i);
  newSpreadExtraArgs(i);
  newSpreadExtraArgsBail(i);
  setArgs(i, i);
  setArgsBail(i, i);

  recoverLength(i);
  recoverContent(i);
  recoverApply(i);
  recoverSpread(i);
  recoverNewSpread(i);
  recoverSetArgs(i, "ok");
}
