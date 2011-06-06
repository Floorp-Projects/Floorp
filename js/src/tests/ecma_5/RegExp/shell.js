function makeExpectedMatch(arr, index, input) {
    var expectedMatch = {
        index: index,
        input: input,
        length: arr.length,
    };

    for (var i = 0; i < arr.length; ++i)
        expectedMatch[i] = arr[i];

    return expectedMatch;
}

function checkRegExpMatch(actual, expected) {
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; ++i)
        assertEq(actual[i], expected[i]);

    assertEq(actual.index, expected.index);
    assertEq(actual.input, expected.input);
}
