gczeal(0);
enableShellAllocationMetadataBuilder();
setMarkStackLimit(1);
Function('gc()'.replace(/x/))();
