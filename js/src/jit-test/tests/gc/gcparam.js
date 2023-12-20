gczeal(0);

function testGetParam(key) {
  gcparam(key);
}

function testChangeParam(key, diff) {
  if (!diff) {
    diff = 1;
  }
  
  let prev = gcparam(key);

  let newValue = prev > 0 ? prev - diff : prev + diff;
  gcparam(key, newValue);
  assertEq(gcparam(key), newValue);

  gcparam(key, prev);
  assertEq(gcparam(key), prev);
}

testGetParam("gcBytes");
testGetParam("gcNumber");
testGetParam("unusedChunks");
testGetParam("totalChunks");
testGetParam("nurseryBytes");
testGetParam("majorGCNumber");
testGetParam("minorGCNumber");
testGetParam("chunkBytes");
testGetParam("helperThreadCount");

testChangeParam("maxBytes");
testChangeParam("minNurseryBytes", 16 * 1024);
testChangeParam("maxNurseryBytes", 1024 * 1024);
testChangeParam("incrementalGCEnabled");
testChangeParam("perZoneGCEnabled");
testChangeParam("sliceTimeBudgetMS");
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
testChangeParam("parallelMarkingEnabled");
testChangeParam("parallelMarkingThresholdMB");
testChangeParam("minLastDitchGCPeriod");
testChangeParam("nurseryFreeThresholdForIdleCollection");
testChangeParam("nurseryFreeThresholdForIdleCollectionPercent");
testChangeParam("nurseryTimeoutForIdleCollectionMS");
testChangeParam("zoneAllocDelayKB");
testChangeParam("mallocThresholdBase");
testChangeParam("urgentThreshold");
testChangeParam("nurseryTimeoutForIdleCollectionMS");
testChangeParam("helperThreadRatio");
testChangeParam("maxHelperThreads");
