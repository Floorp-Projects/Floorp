const tests = [
  () => evalStencil(1),
  () => evalStencil({}),
  () => evalStencil([]),
  () => evalStencilXDR(1),
  () => evalStencilXDR({}),
  () => evalStencilXDR([]),
  () => instantiateModuleStencil(1),
  () => instantiateModuleStencil({}),
  () => instantiateModuleStencil([]),
  () => instantiateModuleStencilXDR(1),
  () => instantiateModuleStencilXDR({}),
  () => instantiateModuleStencilXDR([]),
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
