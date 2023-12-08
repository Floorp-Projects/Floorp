
// This test case is used to test one common configuration seen in the wild and
// that we expect to be successful at optimizing properly.

// Similar functions are part of popular framework such as React and Angular.
function shallowEqual(o1, o2) {
    var k1 = Object.keys(o1);
    var k2 = Object.keys(o2);
    if (k1.length != k2.length) {
        return false;
    }
    for (var k = 0; k < k1.length; k++) {
        if (!Object.hasOwnProperty.call(o2, k1[k]) || !Object.is(o1[k1[k]], o2[k1[k]])) {
            return false;
        }
    }
    return true;
}

let objs = [
    {x:0, a: 1, b: 2},
    {x:1, b: 1, c: 2},
    {x:2, c: 1, d: 2},
    {x:3, a: 1, b: 2, c: 3},
    {x:4, b: 1, c: 2, d: 3},
    {x:5, a: 1, b: 2, c: 3, d: 4}
];

with({}) {}
for (let i = 0; i < 1000; i++) {
    for (let o1 of objs) {
        for (let o2 of objs) {
            assertEq(shallowEqual(o1, o2), Object.is(o1, o2));
        }
    }
}
