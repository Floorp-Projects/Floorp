g1 = newGlobal({ sameZoneAs: this });

// Turn on the object metadata builder because it will create internal weakmap
// entries for all objects created from here on.
enableShellAllocationMetadataBuilder();

// Construct a CCW for g1.Object, which will internally create the object
// metadata table and add an entry to it.
g1.Object;

// Begin an incremental GC
gczeal(0);
startgc(1);

// Call a scary function that does weird stuff.
recomputeWrappers();
