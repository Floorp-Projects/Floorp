// |jit-test| skip-if: !('gczeal' in this)

gczeal(15);
enableShellAllocationMetadataBuilder();
var registry = new FinalizationRegistry(x => 0);
gczeal(9, 3);
registry.register({}, 1, {});
