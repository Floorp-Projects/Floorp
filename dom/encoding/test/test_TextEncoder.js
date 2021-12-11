/*
 * test_TextEncoder.js
 * bug 764234 tests
 */

function runTextEncoderTests() {
  test(testEncoderEncode, "testEncoderEncode");
  test(testEncoderGetEncoding, "testEncoderGetEncoding");
  test(testInvalidSequence, "testInvalidSequence");
  test(testInputString, "testInputString");
  test(testStreamingOptions, "testStreamingOptions");
}

function testEncoderEncode() {
  var data =
    "\u00a0\u0e01\u0e02\u0e03\u0e04\u0e05\u0e06\u0e07\u0e08\u0e09" +
    "\u0e0a\u0e0b\u0e0c\u0e0d\u0e0e\u0e0f\u0e10\u0e11\u0e12\u0e13\u0e14" +
    "\u0e15\u0e16\u0e17\u0e18\u0e19\u0e1a\u0e1b\u0e1c\u0e1d\u0e1e\u0e1f" +
    "\u0e20\u0e21\u0e22\u0e23\u0e24\u0e25\u0e26\u0e27\u0e28\u0e29\u0e2a" +
    "\u0e2b\u0e2c\u0e2d\u0e2e\u0e2f\u0e30\u0e31\u0e32\u0e33\u0e34\u0e35" +
    "\u0e36\u0e37\u0e38\u0e39\u0e3a\u0e3f\u0e40\u0e41\u0e42\u0e43\u0e44" +
    "\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a\u0e4b\u0e4c\u0e4d\u0e4e\u0e4f" +
    "\u0e50\u0e51\u0e52\u0e53\u0e54\u0e55\u0e56\u0e57\u0e58\u0e59\u0e5a" +
    "\u0e5b";

  var expectedString = [
    0xc2,
    0xa0,
    0xe0,
    0xb8,
    0x81,
    0xe0,
    0xb8,
    0x82,
    0xe0,
    0xb8,
    0x83,
    0xe0,
    0xb8,
    0x84,
    0xe0,
    0xb8,
    0x85,
    0xe0,
    0xb8,
    0x86,
    0xe0,
    0xb8,
    0x87,
    0xe0,
    0xb8,
    0x88,
    0xe0,
    0xb8,
    0x89,
    0xe0,
    0xb8,
    0x8a,
    0xe0,
    0xb8,
    0x8b,
    0xe0,
    0xb8,
    0x8c,
    0xe0,
    0xb8,
    0x8d,
    0xe0,
    0xb8,
    0x8e,
    0xe0,
    0xb8,
    0x8f,
    0xe0,
    0xb8,
    0x90,
    0xe0,
    0xb8,
    0x91,
    0xe0,
    0xb8,
    0x92,
    0xe0,
    0xb8,
    0x93,
    0xe0,
    0xb8,
    0x94,
    0xe0,
    0xb8,
    0x95,
    0xe0,
    0xb8,
    0x96,
    0xe0,
    0xb8,
    0x97,
    0xe0,
    0xb8,
    0x98,
    0xe0,
    0xb8,
    0x99,
    0xe0,
    0xb8,
    0x9a,
    0xe0,
    0xb8,
    0x9b,
    0xe0,
    0xb8,
    0x9c,
    0xe0,
    0xb8,
    0x9d,
    0xe0,
    0xb8,
    0x9e,
    0xe0,
    0xb8,
    0x9f,
    0xe0,
    0xb8,
    0xa0,
    0xe0,
    0xb8,
    0xa1,
    0xe0,
    0xb8,
    0xa2,
    0xe0,
    0xb8,
    0xa3,
    0xe0,
    0xb8,
    0xa4,
    0xe0,
    0xb8,
    0xa5,
    0xe0,
    0xb8,
    0xa6,
    0xe0,
    0xb8,
    0xa7,
    0xe0,
    0xb8,
    0xa8,
    0xe0,
    0xb8,
    0xa9,
    0xe0,
    0xb8,
    0xaa,
    0xe0,
    0xb8,
    0xab,
    0xe0,
    0xb8,
    0xac,
    0xe0,
    0xb8,
    0xad,
    0xe0,
    0xb8,
    0xae,
    0xe0,
    0xb8,
    0xaf,
    0xe0,
    0xb8,
    0xb0,
    0xe0,
    0xb8,
    0xb1,
    0xe0,
    0xb8,
    0xb2,
    0xe0,
    0xb8,
    0xb3,
    0xe0,
    0xb8,
    0xb4,
    0xe0,
    0xb8,
    0xb5,
    0xe0,
    0xb8,
    0xb6,
    0xe0,
    0xb8,
    0xb7,
    0xe0,
    0xb8,
    0xb8,
    0xe0,
    0xb8,
    0xb9,
    0xe0,
    0xb8,
    0xba,
    0xe0,
    0xb8,
    0xbf,
    0xe0,
    0xb9,
    0x80,
    0xe0,
    0xb9,
    0x81,
    0xe0,
    0xb9,
    0x82,
    0xe0,
    0xb9,
    0x83,
    0xe0,
    0xb9,
    0x84,
    0xe0,
    0xb9,
    0x85,
    0xe0,
    0xb9,
    0x86,
    0xe0,
    0xb9,
    0x87,
    0xe0,
    0xb9,
    0x88,
    0xe0,
    0xb9,
    0x89,
    0xe0,
    0xb9,
    0x8a,
    0xe0,
    0xb9,
    0x8b,
    0xe0,
    0xb9,
    0x8c,
    0xe0,
    0xb9,
    0x8d,
    0xe0,
    0xb9,
    0x8e,
    0xe0,
    0xb9,
    0x8f,
    0xe0,
    0xb9,
    0x90,
    0xe0,
    0xb9,
    0x91,
    0xe0,
    0xb9,
    0x92,
    0xe0,
    0xb9,
    0x93,
    0xe0,
    0xb9,
    0x94,
    0xe0,
    0xb9,
    0x95,
    0xe0,
    0xb9,
    0x96,
    0xe0,
    0xb9,
    0x97,
    0xe0,
    0xb9,
    0x98,
    0xe0,
    0xb9,
    0x99,
    0xe0,
    0xb9,
    0x9a,
    0xe0,
    0xb9,
    0x9b,
  ];

  // valid encoding passed
  testSingleString({
    input: data,
    expected: expectedString,
    msg: "testing encoding with valid utf-8 encoding.",
  });
}

function testInvalidSequence() {
  var data = "\u0e43\u0e44\ufffd\u0e45";
  var expectedString = [
    0xe0,
    0xb9,
    0x83,
    0xe0,
    0xb9,
    0x84,
    0xef,
    0xbf,
    0xbd,
    0xe0,
    0xb9,
    0x85,
  ];

  //Test null input string
  testSingleString({
    input: data,
    expected: expectedString,
    msg: "encoder with replacement character test.",
  });
}

function testInputString() {
  //Test null input string
  testSingleString({
    input: "",
    expected: [],
    msg: "encoder null input string test.",
  });

  //Test spaces as input string
  testSingleString({
    input: "  ",
    expected: [32, 32],
    msg: "spaces as input string.",
  });
}

function testSingleString(test) {
  var outText;
  try {
    var stream = test.stream ? { stream: true } : null;
    outText = new TextEncoder().encode(test.input, stream);
  } catch (e) {
    assert_equals(
      e.name,
      test.error,
      test.msg + " error thrown from the constructor."
    );
    if (test.errorMessage) {
      assert_equals(
        e.message,
        test.errorMessage,
        test.msg + " error thrown from the constructor."
      );
    }
    return;
  }
  assert_true(!test.error, test.msg);

  if (outText.length !== test.expected.length) {
    assert_equals(
      outText.length,
      test.expected.length,
      test.msg + " length mismatch"
    );
    return;
  }

  for (var i = 0; i < outText.length; i++) {
    if (outText[i] != test.expected[i]) {
      assert_equals(
        escape(stringFromArray(outText.buffer)),
        escape(stringFromArray(test.expected)),
        test.msg + " Bytes do not match expected bytes."
      );
      return;
    }
  }
}

function stringFromArray(a) {
  return Array.map
    .call(a, function(v) {
      return String.fromCharCode(v);
    })
    .join("");
}

function testStreamingOptions() {
  var data = [
    "\u00a0\u0e01\u0e02\u0e03\u0e04\u0e05\u0e06\u0e07\u0e08\u0e09\u0e0a" +
      "\u0e0b\u0e0c\u0e0d\u0e0e\u0e0f\u0e10\u0e11\u0e12\u0e13\u0e14" +
      "\u0e15\u0e16\u0e17\u0e18\u0e19\u0e1a\u0e1b\u0e1c\u0e1d",
    "\u0e1e\u0e1f\u0e20\u0e21\u0e22\u0e23\u0e24\u0e25\u0e26\u0e27\u0e28" +
      "\u0e29\u0e2a\u0e2b\u0e2c\u0e2d\u0e2e\u0e2f\u0e30\u0e31\u0e32" +
      "\u0e33\u0e34\u0e35\u0e36\u0e37\u0e38\u0e39\u0e3a\u0e3f\u0e40" +
      "\u0e41\u0e42",
    "\u0e43\u0e44\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a\u0e4b\u0e4c\u0e4d" +
      "\u0e4e\u0e4f\u0e50\u0e51\u0e52\u0e53\u0e54\u0e55\u0e56\u0e57" +
      "\u0e58\u0e59\u0e5a\u0e5b",
  ];

  var expected = [
    [
      0xc2,
      0xa0,
      0xe0,
      0xb8,
      0x81,
      0xe0,
      0xb8,
      0x82,
      0xe0,
      0xb8,
      0x83,
      0xe0,
      0xb8,
      0x84,
      0xe0,
      0xb8,
      0x85,
      0xe0,
      0xb8,
      0x86,
      0xe0,
      0xb8,
      0x87,
      0xe0,
      0xb8,
      0x88,
      0xe0,
      0xb8,
      0x89,
      0xe0,
      0xb8,
      0x8a,
      0xe0,
      0xb8,
      0x8b,
      0xe0,
      0xb8,
      0x8c,
      0xe0,
      0xb8,
      0x8d,
      0xe0,
      0xb8,
      0x8e,
      0xe0,
      0xb8,
      0x8f,
      0xe0,
      0xb8,
      0x90,
      0xe0,
      0xb8,
      0x91,
      0xe0,
      0xb8,
      0x92,
      0xe0,
      0xb8,
      0x93,
      0xe0,
      0xb8,
      0x94,
      0xe0,
      0xb8,
      0x95,
      0xe0,
      0xb8,
      0x96,
      0xe0,
      0xb8,
      0x97,
      0xe0,
      0xb8,
      0x98,
      0xe0,
      0xb8,
      0x99,
      0xe0,
      0xb8,
      0x9a,
      0xe0,
      0xb8,
      0x9b,
      0xe0,
      0xb8,
      0x9c,
      0xe0,
      0xb8,
      0x9d,
    ],

    [
      0xe0,
      0xb8,
      0x9e,
      0xe0,
      0xb8,
      0x9f,
      0xe0,
      0xb8,
      0xa0,
      0xe0,
      0xb8,
      0xa1,
      0xe0,
      0xb8,
      0xa2,
      0xe0,
      0xb8,
      0xa3,
      0xe0,
      0xb8,
      0xa4,
      0xe0,
      0xb8,
      0xa5,
      0xe0,
      0xb8,
      0xa6,
      0xe0,
      0xb8,
      0xa7,
      0xe0,
      0xb8,
      0xa8,
      0xe0,
      0xb8,
      0xa9,
      0xe0,
      0xb8,
      0xaa,
      0xe0,
      0xb8,
      0xab,
      0xe0,
      0xb8,
      0xac,
      0xe0,
      0xb8,
      0xad,
      0xe0,
      0xb8,
      0xae,
      0xe0,
      0xb8,
      0xaf,
      0xe0,
      0xb8,
      0xb0,
      0xe0,
      0xb8,
      0xb1,
      0xe0,
      0xb8,
      0xb2,
      0xe0,
      0xb8,
      0xb3,
      0xe0,
      0xb8,
      0xb4,
      0xe0,
      0xb8,
      0xb5,
      0xe0,
      0xb8,
      0xb6,
      0xe0,
      0xb8,
      0xb7,
      0xe0,
      0xb8,
      0xb8,
      0xe0,
      0xb8,
      0xb9,
      0xe0,
      0xb8,
      0xba,
      0xe0,
      0xb8,
      0xbf,
      0xe0,
      0xb9,
      0x80,
      0xe0,
      0xb9,
      0x81,
      0xe0,
      0xb9,
      0x82,
    ],

    [
      0xe0,
      0xb9,
      0x83,
      0xe0,
      0xb9,
      0x84,
      0xe0,
      0xb9,
      0x85,
      0xe0,
      0xb9,
      0x86,
      0xe0,
      0xb9,
      0x87,
      0xe0,
      0xb9,
      0x88,
      0xe0,
      0xb9,
      0x89,
      0xe0,
      0xb9,
      0x8a,
      0xe0,
      0xb9,
      0x8b,
      0xe0,
      0xb9,
      0x8c,
      0xe0,
      0xb9,
      0x8d,
      0xe0,
      0xb9,
      0x8e,
      0xe0,
      0xb9,
      0x8f,
      0xe0,
      0xb9,
      0x90,
      0xe0,
      0xb9,
      0x91,
      0xe0,
      0xb9,
      0x92,
      0xe0,
      0xb9,
      0x93,
      0xe0,
      0xb9,
      0x94,
      0xe0,
      0xb9,
      0x95,
      0xe0,
      0xb9,
      0x96,
      0xe0,
      0xb9,
      0x97,
      0xe0,
      0xb9,
      0x98,
      0xe0,
      0xb9,
      0x99,
      0xe0,
      0xb9,
      0x9a,
      0xe0,
      0xb9,
      0x9b,
    ],
  ];

  // STREAMING TEST ONE: test streaming three valid strings with stream option
  // set to true for all three.
  testArrayOfStrings({
    array: [
      { input: data[0], stream: true, expected: expected[0] },
      { input: data[1], stream: true, expected: expected[1] },
      { input: data[2], stream: true, expected: expected[2] },
    ],
    msg: "streaming test one.",
  });

  // STREAMING TEST TWO: test streaming valid strings with stream option
  // streaming option: false from constructor, string 1 stream: true,
  // string 2 stream: false, string 3 stream: false
  testArrayOfStrings({
    array: [
      { input: data[0], stream: true, expected: expected[0] },
      { input: data[1], expected: expected[1] },
      { input: data[2], expected: expected[2] },
    ],
    msg: "streaming test two.",
  });
}

function arrayFromString(s) {
  return s.split("").map(function(c) {
    return c.charCodeAt(0);
  });
}

function testArrayOfStrings(test) {
  var encoder;
  try {
    encoder = new TextEncoder();
  } catch (e) {
    assert_equals(e.name, test.error, test.msg);
    return;
  }
  assert_true(!test.error, test.msg);

  var array = test.array;
  for (var i = 0; i < array.length; i += 1) {
    var stream = array[i].stream ? { stream: true } : null;
    var view = encoder.encode(array[i].input, stream);

    var stringLen = view.length;
    var expected = array[i].expected;
    for (var j = 0; j < stringLen; j++) {
      if (view[j] !== expected[j]) {
        assert_equals(
          view[j],
          expected[j],
          msg + " Bytes do not match expected bytes."
        );
        return;
      }
    }
  }
}

function testEncoderGetEncoding() {
  var encoder = new TextEncoder();
  assert_equals(encoder.encoding, "utf-8", "TextEncoder encoding test.");
}
