load(libdir + "asserts.js");

assertThrowsInstanceOf(function () { readlineBuf() }, Error);

var testBuffers = [
    "foo\nbar\nbaz\n",
    "foo\nbar\nbaz",
    "foo\n\nbar\nbaz",
    "f",
    "\n",
    "\nf",
    ""
];

var expected = [
    [ "foo", "bar", "baz" ],
    [ "foo", "bar", "baz" ],
    [ "foo", "", "bar", "baz" ],
    [ "f" ],
    [ "" ],
    [ "", "f" ],
    []
];

for (var idx in testBuffers) {
    readlineBuf(testBuffers[idx]);
    var result = [];

    while ((line = readlineBuf()) != null) {
        result.push(line);
    }

    assertDeepEq(result, expected[idx]);
}

readlineBuf(testBuffers[0]);
readlineBuf();
readlineBuf();
readlineBuf(testBuffers[3]);
assertEq(readlineBuf(), expected[3][0]);
