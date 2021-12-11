// Undefined and NaN end up as zero after ToUint32
assertEq(Math.clz32(), 32);
assertEq(Math.clz32(NaN), 32);
assertEq(Math.clz32.call(), 32);
// 0
assertEq(Math.clz32(null), 32);
assertEq(Math.clz32(false), 32);
// 1
assertEq(Math.clz32(true), 31);
// 3
assertEq(Math.clz32(3.5), 30);
// NaN -> 0
assertEq(Math.clz32({}), 32);
// 2
assertEq(Math.clz32({valueOf: function() { return 2; }}), 30);
// 0 -> 0
assertEq(Math.clz32([]), 32);
assertEq(Math.clz32(""), 32);
// NaN -> 0
assertEq(Math.clz32([1, 2, 3]), 32);
assertEq(Math.clz32("bar"), 32);
// 15
assertEq(Math.clz32("15"), 28);


assertEq(Math.clz32(0x80000000), 0);
assertEq(Math.clz32(0xF0FF1000), 0);
assertEq(Math.clz32(0x7F8F0001), 1);
assertEq(Math.clz32(0x3FFF0100), 2);
assertEq(Math.clz32(0x1FF50010), 3);
assertEq(Math.clz32(0x00800000), 8);
assertEq(Math.clz32(0x00400000), 9);
assertEq(Math.clz32(0x00008000), 16);
assertEq(Math.clz32(0x00004000), 17);
assertEq(Math.clz32(0x00000080), 24);
assertEq(Math.clz32(0x00000040), 25);
assertEq(Math.clz32(0x00000001), 31);
assertEq(Math.clz32(0), 32);

reportCompare(0, 0, 'ok');
