let badLengths = [
    -1,
    1.0001342,
    0x100000000,
    0x200000000,
    0x400000000,
    Infinity,
    NaN
];

for (let n of badLengths)
    assertThrowsInstanceOf(() => new ArrayBuffer(n), RangeError);

assertEq(new ArrayBuffer({valueOf: () => 123}).byteLength, 123);
assertEq(new ArrayBuffer(true).byteLength, 1);
assertEq(new ArrayBuffer([123]).byteLength, 123);
assertEq(new ArrayBuffer(["0x10"]).byteLength, 16);

if (typeof reportCompare === 'function')
    reportCompare(0, 0, "ok");
