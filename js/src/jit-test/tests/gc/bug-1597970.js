// |jit-test| --enable-weak-refs
enableShellAllocationMetadataBuilder();
evaluate(`
  gczeal(9,3);
  new FinalizationRegistry(function() {});
`);
