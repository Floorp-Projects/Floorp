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
testChangeParam("maxMallocBytes");
testChangeParam("mode");
testChangeParam("sliceTimeBudget");
testChangeParam("markStackLimit");
testChangeParam("highFrequencyTimeLimit");
testChangeParam("highFrequencyLowLimit");
testChangeParam("highFrequencyHighLimit");
testChangeParam("highFrequencyHeapGrowthMax");
testChangeParam("highFrequencyHeapGrowthMin");
testChangeParam("lowFrequencyHeapGrowth");
testChangeParam("dynamicHeapGrowth");
testChangeParam("dynamicMarkSlice");
testChangeParam("allocationThreshold");
testChangeParam("minEmptyChunkCount");
testChangeParam("maxEmptyChunkCount");
testChangeParam("compactingEnabled");

testMBParamValue("highFrequencyLowLimit");
testMBParamValue("highFrequencyHighLimit");
