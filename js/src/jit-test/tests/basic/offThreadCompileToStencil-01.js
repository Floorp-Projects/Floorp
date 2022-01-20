// |jit-test| skip-if: helperThreadCount() === 0

// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Test off-thread parsing.

load(libdir + 'asserts.js');

offThreadCompileToStencil('Math.sin(Math.PI/2)');
var stencil = finishOffThreadCompileToStencil();
assertEq(evalStencil(stencil), 1);

offThreadCompileToStencil('a string which cannot be reduced to the start symbol');
assertThrowsInstanceOf(() => {
  var stencil = finishOffThreadCompileToStencil();
  evalStencil(stencil);
}, SyntaxError);

offThreadCompileToStencil('smerg;');
assertThrowsInstanceOf(() => {
  var stencil = finishOffThreadCompileToStencil();
  evalStencil(stencil);
}, ReferenceError);

offThreadCompileToStencil('throw "blerg";');
assertThrowsValue(() => {
  var stencil = finishOffThreadCompileToStencil();
  evalStencil(stencil);
}, 'blerg');
