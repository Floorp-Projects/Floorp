gczeal(0);
enableShellAllocationMetadataBuilder();
gcparam("markStackLimit",1);
Function('gc()'.replace(/x/))();
