// |jit-test| skip-if: helperThreadCount() === 0

// Test offThreadCompileScript for different function types.

load(libdir + 'asserts.js');

var id;

id = offThreadCompileScript(`
  function f() { return "pass"; }
  f();
`);
assertEq(runOffThreadScript(id), "pass");

id = offThreadCompileScript(`
  function* f() { return "pass"; }
  f().next();
`);
assertDeepEq(runOffThreadScript(id), {value: "pass", done: true});

id = offThreadCompileScript(`
  async function f() { return "pass"; }
  f();
`);
assertEventuallyEq(runOffThreadScript(id), "pass");

id = offThreadCompileScript(`
  async function* f() { return "pass"; }
  f().next();
`);
assertEventuallyDeepEq(runOffThreadScript(id), {value: "pass", done: true});

// Copied from js/src/tests/shell.js
function getPromiseResult(promise) {
  var result, error, caught = false;
  promise.then(r => { result = r; },
               e => { caught = true; error = e; });
  drainJobQueue();
  if (caught)
    throw error;
  return result;
}

function assertEventuallyEq(promise, expected) {
  assertEq(getPromiseResult(promise), expected);
}

function assertEventuallyDeepEq(promise, expected) {
  assertDeepEq(getPromiseResult(promise), expected);
}
