function test(script_str) {
  const eval_f = eval;
  const options = {
    fileName: "compileToStencil-DATA.js",
    lineNumber: 1,
    forceFullParse: true,
  };
  const stencil = compileToStencil(script_str, options);
  const result = evalStencil(stencil, options);
  assertEq(result, eval_f(script_str));
}

test(`
var a = 10;
let b = 20, c = 30;
const d = 40;
function f() {
  return a + b + c + d;
}
f();
`);
