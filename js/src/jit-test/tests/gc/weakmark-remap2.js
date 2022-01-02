gczeal(0);

var g1 = newGlobal({ sameZoneAs: this });

// Turn on the object metadata builder because it will create internal weakmap
// entries for all objects created from here on.
enableShellAllocationMetadataBuilder();

// Construct a pair of error objects. Perhaps this fills out the metadata table?
new Error();
new Error();

// Construct a CCW for g1.Object, which will internally create the object
// metadata table and add an entry to it.
try { g1.Object } catch(e) { }

// Begin an incremental GC
startgc(1);

// Call a scary function that does weird stuff.
recomputeWrappers();
