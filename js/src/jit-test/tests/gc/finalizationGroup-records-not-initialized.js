// |jit-test| --enable-weak-refs
enableShellAllocationMetadataBuilder();
evaluate(`
  var group = new FinalizationGroup(x => 0);
  gczeal(9,3);
  group.register({}, 1, {});
`);
