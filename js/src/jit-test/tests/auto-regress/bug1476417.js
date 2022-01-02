function f(x) {
    var y, z;

    arguments; // Force creation of mapped arguments, so modifying |x| writes to the arguments object.
    Math; // Access a global variable to create a resume point.
    z = x + 1; // Was executed twice because only the resume point for 'Math' was present before the fix.
    x = z; // Modifying |x| writes into the arguments object. We missed to create a resume point here.
    y = 2 * x; // Triggers a bailout when overflowing int32 boundaries.

    return [x, y];
}

var x = [0, 0, 0x3FFFFFFF];

for (var j = 0; j < 3; ++j) {
    var value = x[j];
    var expected = [value + 1, (value + 1) * 2];
    var actual = f(value);

    assertEq(actual[0], expected[0]);
    assertEq(actual[1], expected[1]);
}
