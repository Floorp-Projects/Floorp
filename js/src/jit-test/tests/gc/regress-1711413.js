// Required --no-threads to crash.

enableShellAllocationMetadataBuilder();
newGlobal({newCompartment:true});
gcslice(1);
nukeAllCCWs();
