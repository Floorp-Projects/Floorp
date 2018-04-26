gcslice(0);
function testChangeParam(key) {
    let prev = gcparam(key);
    gcparam(key, prev);
}
testChangeParam("maxMallocBytes");
