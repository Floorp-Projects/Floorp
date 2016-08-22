function testGetParam(key) {
    gcparam(key);
}

function testChangeParam(key) {
    let prev = gcparam(key);
    let value = prev - 1;
    gcparam(key, value);
    assertEq(gcparam(key), value);
    gcparam(key, prev);
}

function testLargeParamValue(key) {
    let prev = gcparam(key);
    const value = 1000000;
    gcparam(key, value);
    assertEq(gcparam(key), value);
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

testLargeParamValue("highFrequencyLowLimit");
testLargeParamValue("highFrequencyHighLimit");
