// Deterministic Math.random, from Octane.
var seed = 49734321;
function rand() {
    // Robert Jenkins' 32 bit integer hash function.
    seed = ((seed + 0x7ed55d16) + (seed << 12))  & 0xffffffff;
    seed = ((seed ^ 0xc761c23c) ^ (seed >>> 19)) & 0xffffffff;
    seed = ((seed + 0x165667b1) + (seed << 5))   & 0xffffffff;
    seed = ((seed + 0xd3a2646c) ^ (seed << 9))   & 0xffffffff;
    seed = ((seed + 0xfd7046c5) + (seed << 3))   & 0xffffffff;
    seed = ((seed ^ 0xb55a4f09) ^ (seed >>> 16)) & 0xffffffff;
    return (seed & 0xfffffff) / 0x10000000;
}
var randomS = "";
function test(len) {
    const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    var s = "var data = '";
    while (randomS.length < len) {
        randomS += chars.charAt((rand() * chars.length)|0);
    }
    s += randomS;
    s += "'; function f() {}; assertEq(f.toString(), 'function f() {}');";
    eval(s);
}
for (var i=32740; i<32780; i += 5) {
    test(i);
}
