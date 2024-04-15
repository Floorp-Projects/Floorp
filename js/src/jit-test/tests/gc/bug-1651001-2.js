gczeal(0);
g1 = newGlobal({ sameZoneAs: this });
enableShellAllocationMetadataBuilder();
g1.Object;
startgc(1);
recomputeWrappers();
