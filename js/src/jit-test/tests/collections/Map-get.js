// Map.prototype.get and .has basically work
var m = new Map;

function rope() {
    var s = "s";
    for (var i = 0; i < 16; i++)
        s += s;
    return s;
}

var keys = [undefined, null, true, false,
            0, 1, 65535, 65536, 2147483647, 2147483648, 4294967295, 4294967296,
            -1, -65536, -2147483648,
            0.5, Math.sqrt(81), Math.PI,
            Number.MAX_VALUE, -Number.MAX_VALUE, Number.MIN_VALUE, -Number.MIN_VALUE,
            NaN, Infinity, -Infinity,
            "", "\0", "a", "ab", "abcdefg", rope(),
            {}, [], /a*b/, Object.prototype, Object];

for (var i = 0; i < keys.length; i++) {
    // without being set
    var key = keys[i];
    assertEq(m.has(key), false);
    assertEq(m.get(key), undefined);

    // after being set
    var v = {};
    assertEq(m.set(key, v), m);
    assertEq(m.has(key), true);
    assertEq(m.get(key), v);

    // after being deleted
    assertEq(m.delete(key), true);
    assertEq(m.has(key), false);
    assertEq(m.get(key), undefined);

    m.set(key, v);
}

for (var i = 0; i < keys.length; i++)
    assertEq(m.has(keys[i]), true);
