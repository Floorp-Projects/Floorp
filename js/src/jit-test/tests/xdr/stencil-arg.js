const tests = [
  () => evalStencil(1),
  () => evalStencil({}),
  () => evalStencilXDR(1),
  () => evalStencilXDR({}),
  () => instantiateModuleStencil(1),
  () => instantiateModuleStencil({}),
  () => instantiateModuleStencilXDR(1),
  () => instantiateModuleStencilXDR({}),
];

for (const test of tests) {
  let caught = false;
  try {
    test();
  } catch (e) {
    assertEq(/Stencil( XDR)? object expected/.test(e.message), true);
    caught = true;
  }
  assertEq(caught, true);
}
