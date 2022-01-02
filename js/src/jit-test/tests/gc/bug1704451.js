// |jit-test| skip-if: !('gczeal' in this)

enableShellAllocationMetadataBuilder();
gczeal(9,1);
var o86 = {x76: 1, y86: 2};
var snapshot = createShapeSnapshot(o86);
