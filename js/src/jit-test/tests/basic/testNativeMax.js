function testNativeMax() {
    var out = [], k;
    for (var i = 0; i < 5; ++i) {
        k = Math.max(k, i);
    }
    out.push(k);

    k = 0;
    for (var i = 0; i < 5; ++i) {
        k = Math.max(k, i);
    }
    out.push(k);

    for (var i = 0; i < 5; ++i) {
        k = Math.max(0, -0);
    }
    out.push((1 / k) < 0);
    return out.join(",");
}
assertEq(testNativeMax(), "NaN,4,false");
