// |jit-test| skip-if: helperThreadCount() === 0

// Test offThreadCompileToStencil for different function types.

load(libdir + 'asserts.js');

var id, stencil;

id = offThreadCompileToStencil(`
  function f() { return "pass"; }
  f();
`);
stencil = finishOffThreadStencil(id);
assertEq(evalStencil(stencil), "pass");

id = offThreadCompileToStencil(`
  function* f() { return "pass"; }
  f().next();
`);
stencil = finishOffThreadStencil(id);
assertDeepEq(evalStencil(stencil), {value: "pass", done: true});

id = offThreadCompileToStencil(`
  async function f() { return "pass"; }
  f();
`);
stencil = finishOffThreadStencil(id);
assertEventuallyEq(evalStencil(stencil), "pass");

id = offThreadCompileToStencil(`
  async function* f() { return "pass"; }
  f().next();
`);
stencil = finishOffThreadStencil(id);
assertEventuallyDeepEq(evalStencil(stencil), {value: "pass", done: true});

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
