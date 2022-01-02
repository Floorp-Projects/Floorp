// |jit-test| skip-if: helperThreadCount() === 0

// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Test off-thread parsing.

load(libdir + 'asserts.js');

offThreadCompileScript('Math.sin(Math.PI/2)');
assertEq(runOffThreadScript(), 1);

offThreadCompileScript('a string which cannot be reduced to the start symbol');
assertThrowsInstanceOf(runOffThreadScript, SyntaxError);

offThreadCompileScript('smerg;');
assertThrowsInstanceOf(runOffThreadScript, ReferenceError);

offThreadCompileScript('throw "blerg";');
assertThrowsValue(runOffThreadScript, 'blerg');
