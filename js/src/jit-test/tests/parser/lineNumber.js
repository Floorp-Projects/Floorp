
// Simple tests for evaluate's "lineNumber" option.

load(libdir + 'asserts.js');

const maxLine = Math.pow(2,32) - 1;

assertEq(evaluate("saveStack().line"), 1);
assertEq(evaluate("saveStack().line", { lineNumber: maxLine }), maxLine);
assertEq(evaluate("\nsaveStack().line"), 2);
assertEq(evaluate("\nsaveStack().line", { lineNumber: 1000 }), 1001);
assertThrowsInstanceOf(() => evaluate("\nsaveStack().line", { lineNumber: maxLine }),
                       RangeError);

if (helperThreadCount() > 0) {
  offThreadCompileToStencil("saveStack().line");
  var stencil = finishOffThreadStencil();
  assertEq(evalStencil(stencil), 1);

  offThreadCompileToStencil("saveStack().line", { lineNumber: maxLine });
  stencil = finishOffThreadStencil();
  assertEq(evalStencil(stencil), maxLine);

  offThreadCompileToStencil("\nsaveStack().line");
  stencil = finishOffThreadStencil();
  assertEq(evalStencil(stencil), 2);

  offThreadCompileToStencil("\nsaveStack().line", { lineNumber: 1000 });
  stencil = finishOffThreadStencil();
  assertEq(evalStencil(stencil), 1001);

  offThreadCompileToStencil("\nsaveStack().line", { lineNumber: maxLine });
  assertThrowsInstanceOf(() => {
    stencil = finishOffThreadStencil();
    evalStencil(stencil);
  }, RangeError);
}
