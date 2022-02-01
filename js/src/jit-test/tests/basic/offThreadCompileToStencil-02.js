// |jit-test| skip-if: helperThreadCount() === 0

// Test offThreadCompileToStencil/evalStencil option handling.

offThreadCompileToStencil("Error()");
var stencil = finishOffThreadCompileToStencil();
assertEq(!!evalStencil(stencil).stack.match(/^@<string>:1:1\n/), true);

offThreadCompileToStencil("Error()", { fileName: "candelabra", lineNumber: 6502 });
stencil = finishOffThreadCompileToStencil();
assertEq(!!evalStencil(stencil).stack.match(/^@candelabra:6502:1\n/), true);

var element = {};
offThreadCompileToStencil("Error()"); // shouldn't crash
stencil = finishOffThreadCompileToStencil();
evalStencil(stencil, { element });

var elementAttributeName = "molybdenum";
elementAttributeName +=
  elementAttributeName + elementAttributeName + elementAttributeName;
offThreadCompileToStencil("Error()"); // shouldn't crash
stencil = finishOffThreadCompileToStencil();
evalStencil(stencil, {
  elementAttributeName,
});
