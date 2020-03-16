// |jit-test| --enable-weak-refs
enableShellAllocationMetadataBuilder();
evaluate(`
  var registry = new FinalizationRegistry(x => 0);
  gczeal(9,3);
  registry.register({}, 1, {});
`);
