var o = {valueOf: function() { return -0x80000000; }};
var s = {valueOf: function() { return 0; }};

for (var i = 0; i < 70; i++) {
    assertEq(o >>> 1, 0x40000000);
    assertEq(o >>> 0, 0x80000000);
    assertEq(1 >>> s, 1);
    assertEq(-1 >>> s, 0xffffffff);
}
