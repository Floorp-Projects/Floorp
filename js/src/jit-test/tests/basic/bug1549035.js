var expected = 2;
for (var i = 0; i < 100; ++i) {
    if (i === 50) {
        expected = 0;
        String.prototype[Symbol.split] = function() {
            return [];
        };
    }
    var r = "ab".split("");
    assertEq(r.length, expected);
}
