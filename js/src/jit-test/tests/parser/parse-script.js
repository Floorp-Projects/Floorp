// |jit-test| skip-if: !('oomTest' in this)

load(libdir + "asserts.js");

let stencil = compileToStencil('314;');
assertEq(evalStencil(stencil), 314);

stencil = compileToStencil('let o = { a: 42, b: 2718 }["b"]; o', { prepareForInstantiate: true });
assertEq(evalStencil(stencil), 2718);

assertThrowsInstanceOf(() => compileToStencil('let fail ='), SyntaxError);
assertThrowsInstanceOf(() => compileToStencil('42;', 42), Error);

oomTest(function() {
    compileToStencil('"hello stencil!";', { prepareForInstantiate: false });
});

oomTest(function() {
    compileToStencil('"hello stencil!";', { prepareForInstantiate: true });
});
