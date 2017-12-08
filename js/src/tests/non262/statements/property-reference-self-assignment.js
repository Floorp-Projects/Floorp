var hits = 0;

var p = { toString() { hits++; return "prop" } };
var obj = { foo: 1 };


var ops = [["obj[p]++", 2],
           ["++obj[p]", 2],
           ["--obj[p]", 0],
           ["obj[p]--", 0],
           ["obj[p] += 2", 3],
           ["obj[p] -= 2", -1],
           ["obj[p] *= 2", 2],
           ["obj[p] /= 2", 0.5],
           ["obj[p] %= 2", 1],
           ["obj[p] >>>= 2", 0],
           ["obj[p] >>= 2", 0],
           ["obj[p] <<= 2", 4],
           ["obj[p] |= 2", 3],
           ["obj[p] ^= 2", 3],
           ["obj[p] &= 2", 0]];

var testHits = 0;
for (let op of ops) {
    // Seed the value for each test.
    obj.prop = 1;

    // Do the operation.
    eval(op[0]);
    assertEq(obj.prop, op[1]);

    // We should always call toString once, for each operation.
    testHits++;
    assertEq(hits, testHits);
}

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
