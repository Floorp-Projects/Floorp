enableShellAllocationMetadataBuilder();
evaluate(`
  gczeal(9,3);
  new FinalizationRegistry(function() {});
`);
