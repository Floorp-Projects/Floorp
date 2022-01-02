function testIter(v) {
    var c = 0;
    for (var p in v) {
        c++;
    }
    assertEq(c === 0 || c === 1, true);
    assertEq(c === 0, v === null || v === undefined);
}
function test() {
    var vals = [{a: 1}, {b: 1}, {c: 1}, {d: 1}, null, undefined,
                {e: 1}, {f: 1}, {g: 1}, {h: 1}, {i: 1}];
    for (var i = 0; i < 100; i++) {
        for (var v of vals) {
            testIter(v);
        }
    }
}
test();
