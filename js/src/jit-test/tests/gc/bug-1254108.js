|jit-test| error: Error
function testChangeParam(key) {
    gcparam(key, 0x22222222);
}
testChangeParam("lowFrequencyHeapGrowth");
