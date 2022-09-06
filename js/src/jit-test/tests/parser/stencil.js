const optionsFull = {
  fileName: "compileToStencil-DATA.js",
  lineNumber: 1,
  eagerDelazificationStrategy: "ParseEverythingEagerly",
};

const optionsLazy = {
  fileName: "compileToStencil-DATA.js",
  lineNumber: 1,
  eagerDelazificationStrategy: "OnDemandOnly",
};

const optionsLazyCache = {
  fileName: "compileToStencil-DATA.js",
  lineNumber: 1,
  eagerDelazificationStrategy: "ConcurrentDepthFirst",
};

function testMainThread(script_str) {
  const eval_f = eval;
  const stencil = compileToStencil(script_str, optionsFull);
  const result = evalStencil(stencil, optionsFull);
  assertEq(result, eval_f(script_str));
}

function testMainThreadDelazifyAll(script_str) {
  if (isLcovEnabled()) {
    // Code-coverage implies forceFullParse = true, and as such it cannot be
    // used while testing to incrementally delazify.
    return;
  }
  const eval_f = eval;
  const stencil = compileToStencil(script_str, optionsLazy);
  const result = evalStencil(stencil, optionsLazy);
  assertEq(result, eval_f(script_str));
}

function testMainThreadCacheAll(script_str) {
  if (isLcovEnabled() || helperThreadCount() === 0) {
    // Code-coverage implies forceFullParse = true, and as such it cannot be
    // used while testing to incrementally delazify.
    // Similarly, concurrent delazification requires off-threads processing.
    return;
  }
  const eval_f = eval;
  const stencil = compileToStencil(script_str, optionsLazyCache);
  const result = evalStencil(stencil, optionsLazyCache);
  assertEq(result, eval_f(script_str));
}

function testOffThread(script_str) {
  const eval_f = eval;
  const job = offThreadCompileToStencil(script_str, optionsFull);
  const stencil = finishOffThreadStencil(job);
  const result = evalStencil(stencil, optionsFull);
  assertEq(result, eval_f(script_str));
}

testMainThread(`
var a = 10;
let b = 20, c = 30;
const d = 40;
function f() {
  return a + b + c + d;
}
f();
`);

testMainThreadDelazifyAll(`
var a1 = 10;
let b1 = 20, c1 = 30;
const d1 = 40;
function g1() {
  function h1() {
    return a1 + b1;
  }
  return h1() + c1;
}
function f1() {
  return a1 + b1 + c1 + d1;
}
f1();
`);

testMainThreadCacheAll(`
var a3 = 10;
let b3 = 20, c3 = 30;
const d3 = 40;
function g3() {
  function h3() {
    return a3 + b3;
  }
  return h3() + c3;
}
function f3() {
  return a3 + b3 + c3 + d3;
}
f3();
`);

if (helperThreadCount() > 0) {
  testOffThread(`
var a2 = 10;
let b2 = 20, c2 = 30;
const d2 = 40;
function f2() {
  return a2 + b2 + c2 + d2;
}
f2();
`);
}
