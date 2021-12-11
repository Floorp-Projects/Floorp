  const options = {
    fileName: "compileToStencil-DATA.js",
    lineNumber: 1,
    forceFullParse: true,
  };

function testMainThread(script_str) {
  const eval_f = eval;
  const stencil = compileToStencil(script_str, options);
  const result = evalStencil(stencil, options);
  assertEq(result, eval_f(script_str));
}

function testOffThread(script_str) {
  const eval_f = eval;
  const job = offThreadCompileToStencil(script_str, options);
  const stencil = finishOffThreadCompileToStencil(job);
  const result = evalStencil(stencil, options);
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
