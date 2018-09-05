load(libdir + "asserts.js");

assertThrowsInstanceOf(function () { readlineBuf() }, Error);

var testBuffers = [
    "foo\nbar\nbaz\n",
    "foo\nbar\nbaz",
    "foo\n\nbar\nbaz",
    "f",
    "\n",
    "\nf",
    "",
    "Ää\n\u{10ffff}",
];

var expected = [
    [ "foo", "bar", "baz" ],
    [ "foo", "bar", "baz" ],
    [ "foo", "", "bar", "baz" ],
    [ "f" ],
    [ "" ],
    [ "", "f" ],
    [],
    ["Ää", "\u{10ffff}"],
];

for (var [idx, testValue] of testBuffers.entries()) {
    readlineBuf(testValue);
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
