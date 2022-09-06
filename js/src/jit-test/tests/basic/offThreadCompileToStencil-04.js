// |jit-test| skip-if: helperThreadCount() === 0

// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Ensure off-thread parse works for ergonomic brand checks.

load(libdir + 'asserts.js');

offThreadCompileToStencil(`
    class A {
        #x
        static hx(o) { return #x in o; }
    };

    throw "Yay"`);

assertThrowsValue(() => {
    var stencil = finishOffThreadStencil();
    evalStencil(stencil);
}, 'Yay');
