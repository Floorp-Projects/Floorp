let threw = false;
try {
    evalStencil([]);
    threw = false;
} catch (e) {
    threw = true;
    assertEq(/Stencil expected/.test(e.message), true);
}
assertEq(threw, true);
