gczeal(0);

function testGetParam(key) {
    gcparam(key);
}

function testChangeParam(key) {
    let prev = gcparam(key);
    let value = prev - 1;
    try {
        gcparam(key, value);
        assertEq(gcparam(key), value);
        gcparam(key, prev);
        assertEq(gcparam(key), prev);
    } catch {
        assertEq(gcparam(key), prev);
    }
}

function testMBParamValue(key) {
    let prev = gcparam(key);
    const value = 1024;
    try {
        gcparam(key, value);
        assertEq(gcparam(key), value);
    } catch {
        assertEq(gcparam(key), prev);
    }
    gcparam(key, prev);
}

testGetParam("gcBytes");
testGetParam("gcNumber");
testGetParam("unusedChunks");
testGetParam("totalChunks");

testChangeParam("maxBytes");
testChangeParam("incrementalGCEnabled");
testChangeParam("perZoneGCEnabled");
testChangeParam("sliceTimeBudgetMS");
testChangeParam("markStackLimit");
testChangeParam("highFrequencyTimeLimit");
testChangeParam("smallHeapSizeMax");
testChangeParam("largeHeapSizeMin");
testChangeParam("highFrequencySmallHeapGrowth");
testChangeParam("highFrequencyLargeHeapGrowth");
testChangeParam("lowFrequencyHeapGrowth");
testChangeParam("balancedHeapLimitsEnabled");
testChangeParam("heapGrowthFactor");
testChangeParam("allocationThreshold");
testChangeParam("smallHeapIncrementalLimit");
testChangeParam("largeHeapIncrementalLimit");
testChangeParam("minEmptyChunkCount");
testChangeParam("maxEmptyChunkCount");
testChangeParam("compactingEnabled");
testChangeParam("mallocThresholdBase");
testChangeParam("urgentThreshold");
testChangeParam("nurseryTimeoutForIdleCollectionMS");

testMBParamValue("smallHeapSizeMax");
testMBParamValue("largeHeapSizeMin");
