// The argument to Set can be a generator.

function hexData(n) {
    for (var i = 0; i < n; i++)
        yield i.toString(16);
}

var s = Set(hexData(256));
assertEq(s.size(), 256);
assertEq(s.has("0"), true);
assertEq(s.has(0), false);
assertEq(s.has("ff"), true);
