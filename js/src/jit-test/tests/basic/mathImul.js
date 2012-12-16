
var table = [
    [NaN, 0, 0],
    [Infinity, Infinity, 0],
    [NaN, 1000, 0],

    [-1, -2, 2],
    [1, 2, 2],
    [-1, 2, -2],
    [1, -2, -2],
    [-0, 0, 0],
    [0, -0, 0],
    [-1, -0, 0],
    [1, -0, 0],

    [0xffffffff, 1, -1],

    [0xffffffff, 0xffffffff, 1],
    [0xffffffff, -0xffffffff, -1],
    [0xffffffff, 0xfffffffe, 2],
    [0xffffffff, -0xfffffffe, -2],
    [0x10000, 0x10000, 0],

    [{}, {}, 0],
    [[], [], 0],
    [{}, [], 0],
    [[], {}, 0],

    [{valueOf: function() { return -1; }}, 0x100000, -1048576],
    ["3", "-4", -12],
    [3.4, 6, 18]
];

try {
    Math.imul({ valueOf: function() { throw "ha ha ha"; } });
    assertEq(true, false, "no error thrown");
} catch (e) {
    assertEq(e, "ha ha ha");
}

var order = [];
assertEq(Math.imul({ valueOf: function() { order.push("first"); return 0; } },
                   { valueOf: function() { order.push("second"); return 0; } }),
         0);
assertEq(order[0], "first");
assertEq(order[1], "second");

var seen = [];
try
{
    Math.imul({ valueOf: function() { seen.push("one"); return 17; } },
              { valueOf: function() { throw "abort!"; } });
    assertEq(true, false, "no error thrown");
}
catch (e)
{
  assertEq(e, "abort!", "should have thrown partway through, instead threw " + e);
}
assertEq(seen.length, 1);
assertEq(seen[0], "one");

assertEq(Math.imul(), 0);
assertEq(Math.imul(100), 0);
assertEq(Math.imul(NaN, 100), 0);
assertEq(Math.imul(NaN, NaN), 0);
assertEq(Math.imul(5, Infinity), 0);

for (var i = 0; i < table.length; i++) {
    assertEq(Math.imul(table[i][0], table[i][1]), table[i][2]);
    assertEq(Math.imul(table[i][1], table[i][0]), table[i][2]);
}
