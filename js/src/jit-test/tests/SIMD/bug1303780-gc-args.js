if (typeof gczeal === 'undefined' || typeof SIMD === 'undefined') {
    quit();
}

gczeal(14,2);
var Float32x4 = SIMD.Float32x4;
function test() {
    var v = Float32x4(1,2,3,4);
    var good = {valueOf: () => 42};
    Float32x4.replaceLane(v, 0, good);
}
test();
