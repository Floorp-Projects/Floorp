try {
    enableShellAllocationMetadataBuilder();
    gczeal(11);
    gczeal(22);
    grayRoot() = 0;
} catch (e) {}
newGlobal();
