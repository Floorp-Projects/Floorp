/*
 * test_TextEncoder.js
 * bug 764234 tests
*/

function runTextEncoderTests()
{
  var data = "\u00a0\u0e01\u0e02\u0e03\u0e04\u0e05\u0e06\u0e07\u0e08\u0e09"
        + "\u0e0a\u0e0b\u0e0c\u0e0d\u0e0e\u0e0f\u0e10\u0e11\u0e12\u0e13\u0e14"
        + "\u0e15\u0e16\u0e17\u0e18\u0e19\u0e1a\u0e1b\u0e1c\u0e1d\u0e1e\u0e1f"
        + "\u0e20\u0e21\u0e22\u0e23\u0e24\u0e25\u0e26\u0e27\u0e28\u0e29\u0e2a"
        + "\u0e2b\u0e2c\u0e2d\u0e2e\u0e2f\u0e30\u0e31\u0e32\u0e33\u0e34\u0e35"
        + "\u0e36\u0e37\u0e38\u0e39\u0e3a\u0e3f\u0e40\u0e41\u0e42\u0e43\u0e44"
        + "\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a\u0e4b\u0e4c\u0e4d\u0e4e\u0e4f"
        + "\u0e50\u0e51\u0e52\u0e53\u0e54\u0e55\u0e56\u0e57\u0e58\u0e59\u0e5a"
        + "\u0e5b";

  var expectedString = [0xC2, 0xA0, 0xE0, 0xB8, 0x81, 0xE0, 0xB8, 0x82, 0xE0,
                       0xB8, 0x83, 0xE0, 0xB8, 0x84, 0xE0, 0xB8, 0x85, 0xE0,
                       0xB8, 0x86, 0xE0, 0xB8, 0x87, 0xE0, 0xB8, 0x88, 0xE0,
                       0xB8, 0x89, 0xE0, 0xB8, 0x8A, 0xE0, 0xB8, 0x8B, 0xE0,
                       0xB8, 0x8C, 0xE0, 0xB8, 0x8D, 0xE0, 0xB8, 0x8E, 0xE0,
                       0xB8, 0x8F, 0xE0, 0xB8, 0x90, 0xE0, 0xB8, 0x91, 0xE0,
                       0xB8, 0x92, 0xE0, 0xB8, 0x93, 0xE0, 0xB8, 0x94, 0xE0,
                       0xB8, 0x95, 0xE0, 0xB8, 0x96, 0xE0, 0xB8, 0x97, 0xE0,
                       0xB8, 0x98, 0xE0, 0xB8, 0x99, 0xE0, 0xB8, 0x9A, 0xE0,
                       0xB8, 0x9B, 0xE0, 0xB8, 0x9C, 0xE0, 0xB8, 0x9D, 0xE0,
                       0xB8, 0x9E, 0xE0, 0xB8, 0x9F, 0xE0, 0xB8, 0xA0, 0xE0,
                       0xB8, 0xA1, 0xE0, 0xB8, 0xA2, 0xE0, 0xB8, 0xA3, 0xE0,
                       0xB8, 0xA4, 0xE0, 0xB8, 0xA5, 0xE0, 0xB8, 0xA6, 0xE0,
                       0xB8, 0xA7, 0xE0, 0xB8, 0xA8, 0xE0, 0xB8, 0xA9, 0xE0,
                       0xB8, 0xAA, 0xE0, 0xB8, 0xAB, 0xE0, 0xB8, 0xAC, 0xE0,
                       0xB8, 0xAD, 0xE0, 0xB8, 0xAE, 0xE0, 0xB8, 0xAF, 0xE0,
                       0xB8, 0xB0, 0xE0, 0xB8, 0xB1, 0xE0, 0xB8, 0xB2, 0xE0,
                       0xB8, 0xB3, 0xE0, 0xB8, 0xB4, 0xE0, 0xB8, 0xB5, 0xE0,
                       0xB8, 0xB6, 0xE0, 0xB8, 0xB7, 0xE0, 0xB8, 0xB8, 0xE0,
                       0xB8, 0xB9, 0xE0, 0xB8, 0xBA, 0xE0, 0xB8, 0xBF, 0xE0,
                       0xB9, 0x80, 0xE0, 0xB9, 0x81, 0xE0, 0xB9, 0x82, 0xE0,
                       0xB9, 0x83, 0xE0, 0xB9, 0x84, 0xE0, 0xB9, 0x85, 0xE0,
                       0xB9, 0x86, 0xE0, 0xB9, 0x87, 0xE0, 0xB9, 0x88, 0xE0,
                       0xB9, 0x89, 0xE0, 0xB9, 0x8A, 0xE0, 0xB9, 0x8B, 0xE0,
                       0xB9, 0x8C, 0xE0, 0xB9, 0x8D, 0xE0, 0xB9, 0x8E, 0xE0,
                       0xB9, 0x8F, 0xE0, 0xB9, 0x90, 0xE0, 0xB9, 0x91, 0xE0,
                       0xB9, 0x92, 0xE0, 0xB9, 0x93, 0xE0, 0xB9, 0x94, 0xE0,
                       0xB9, 0x95, 0xE0, 0xB9, 0x96, 0xE0, 0xB9, 0x97, 0xE0,
                       0xB9, 0x98, 0xE0, 0xB9, 0x99, 0xE0, 0xB9, 0x9A, 0xE0,
                       0xB9, 0x9B];

  test(testEncoderGetEncoding, "testEncoderGetEncoding");
  test(testInvalidSequence, "testInvalidSequence");
  test(testEncodeUTF16ToUTF16, "testEncodeUTF16ToUTF16");
  test(function() {
    testConstructorEncodingOption(data, expectedString)
  }, "testConstructorEncodingOption");
  test(function() {
    testEncodingValues(data, expectedString)
  }, "testEncodingValues");
  test(function() {
    testInputString(data, expectedString)
  }, "testInputString");
  test(testStreamingOptions, "testStreamingOptions");
}

function testInvalidSequence()
{
  var data = "\u0e43\u0e44\ufffd\u0e45";
  var expectedString = [0xE0, 0xB9, 0x83, 0xE0, 0xB9, 0x84, 0xEF, 0xBF, 0xBD,
                        0xE0, 0xB9, 0x85];

  //Test null input string
  testSingleString({encoding: "utf-8", input: data, expected: expectedString,
    msg: "encoder with replacement character test."});
}

function testEncodeUTF16ToUTF16()
{
  var data = "\u0e43\u0e44\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a\u0e4b\u0e4c"
             + "\u0e4d\u0e4e\u0e4f\u0e50\u0e51\u0e52\u0e53\u0e54\u0e55\u0e56"
             + "\u0e57\u0e58\u0e59\u0e5a\u0e5b";

  var expected = [0x43, 0x0E, 0x44, 0x0E, 0x45, 0x0E, 0x46, 0x0E, 0x47, 0x0E,
                  0x48, 0x0E, 0x49, 0x0E, 0x4A, 0x0E, 0x4B, 0x0E, 0x4C, 0x0E,
                  0x4D, 0x0E, 0x4E, 0x0E, 0x4F, 0x0E, 0x50, 0x0E, 0x51, 0x0E,
                  0x52, 0x0E, 0x53, 0x0E, 0x54, 0x0E, 0x55, 0x0E, 0x56, 0x0E,
                  0x57, 0x0E, 0x58, 0x0E, 0x59, 0x0E, 0x5A, 0x0E, 0x5B, 0x0E];

  testSingleString({encoding: "Utf-16", input: data, expected: expected,
    msg: "testing encoding from utf-16 to utf-16 zero."});
}

function testConstructorEncodingOption(aData, aExpectedString)
{
  // valid encoding passed
  testSingleString({encoding: "UTF-8", input: aData, expected: aExpectedString,
    msg: "testing encoding with valid utf-8 encoding."});

  // passing spaces for encoding
  testSingleString({encoding: "   ", input: aData, error: "TypeError",
    msg: "constructor encoding, spaces encoding test."});

  // invalid encoding passed
  testSingleString({encoding: "asdfasdf", input: aData, error: "TypeError",
    msg: "constructor encoding, invalid encoding test."});

  // null encoding passed
  testSingleString({encoding: null, input: aData, error: "TypeError",
    msg: "constructor encoding, \"null\" encoding test."});

  // null encoding passed
  testSingleString({encoding: "", input: aData, error: "TypeError",
    msg: "constructor encoding, empty encoding test."});
}

function testEncodingValues(aData, aExpectedString)
{
  var encoding = "ISO-8859-11";
  testSingleString({encoding: aData, input: encoding, error: "TypeError",
    msg: "encoder encoding values test."});
}

function testInputString(aData, aExpectedString)
{
  //Test null input string
  testSingleString({encoding: "utf-8", input: "", expected: [],
    msg: "encoder null input string test."});

  //Test spaces as input string
  testSingleString({encoding: "utf-8", input: "  ", expected: [32, 32],
    msg: "spaces as input string."});
}

function testSingleString(test)
{
  var outText;
  try {
    var stream = test.stream ? {stream: true} : null;
    outText = (new TextEncoder(test.encoding)).encode(test.input, stream);
  } catch (e) {
    assert_equals(e.name, test.error, test.msg);
    return;
  }
  assert_true(!test.error, test.msg);

  if (outText.length !== test.expected.length) {
    assert_equals(outText.length, test.expected.length, test.msg + " length mismatch");
    return;
  }

  for (var i = 0; i < outText.length; i++) {
    if (outText[i] != test.expected[i]) {
      assert_equals(escape(stringFromArray(outText.buffer)), escape(stringFromArray(test.expected)),
                    test.msg + " Bytes do not match expected bytes.");
      return;
    }
  }
}

function stringFromArray(a) {
  return Array.map.call(a, function(v){return String.fromCharCode(v)}).join('');
}

function testStreamingOptions()
{
  var data = [
    "\u00a0\u0e01\u0e02\u0e03\u0e04\u0e05\u0e06\u0e07\u0e08\u0e09\u0e0a"
    + "\u0e0b\u0e0c\u0e0d\u0e0e\u0e0f\u0e10\u0e11\u0e12\u0e13\u0e14"
    + "\u0e15\u0e16\u0e17\u0e18\u0e19\u0e1a\u0e1b\u0e1c\u0e1d",
    "\u0e1e\u0e1f\u0e20\u0e21\u0e22\u0e23\u0e24\u0e25\u0e26\u0e27\u0e28"
    + "\u0e29\u0e2a\u0e2b\u0e2c\u0e2d\u0e2e\u0e2f\u0e30\u0e31\u0e32"
    + "\u0e33\u0e34\u0e35\u0e36\u0e37\u0e38\u0e39\u0e3a\u0e3f\u0e40"
    + "\u0e41\u0e42",
    "\u0e43\u0e44\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a\u0e4b\u0e4c\u0e4d"
    + "\u0e4e\u0e4f\u0e50\u0e51\u0e52\u0e53\u0e54\u0e55\u0e56\u0e57"
    + "\u0e58\u0e59\u0e5a\u0e5b"
  ];

  var expected = [[0xC2, 0xA0, 0xE0, 0xB8, 0x81, 0xE0, 0xB8, 0x82, 0xE0,
                   0xB8, 0x83, 0xE0, 0xB8, 0x84, 0xE0, 0xB8, 0x85, 0xE0,
                   0xB8, 0x86, 0xE0, 0xB8, 0x87, 0xE0, 0xB8, 0x88, 0xE0,
                   0xB8, 0x89, 0xE0, 0xB8, 0x8A, 0xE0, 0xB8, 0x8B, 0xE0,
                   0xB8, 0x8C, 0xE0, 0xB8, 0x8D, 0xE0, 0xB8, 0x8E, 0xE0,
                   0xB8, 0x8F, 0xE0, 0xB8, 0x90, 0xE0, 0xB8, 0x91, 0xE0,
                   0xB8, 0x92, 0xE0, 0xB8, 0x93, 0xE0, 0xB8, 0x94, 0xE0,
                   0xB8, 0x95, 0xE0, 0xB8, 0x96, 0xE0, 0xB8, 0x97, 0xE0,
                   0xB8, 0x98, 0xE0, 0xB8, 0x99, 0xE0, 0xB8, 0x9A, 0xE0,
                   0xB8, 0x9B, 0xE0, 0xB8, 0x9C, 0xE0, 0xB8, 0x9D],

                  [0xE0, 0xB8, 0x9E, 0xE0, 0xB8, 0x9F, 0xE0, 0xB8, 0xA0,
                   0xE0, 0xB8, 0xA1, 0xE0, 0xB8, 0xA2, 0xE0, 0xB8, 0xA3,
                   0xE0, 0xB8, 0xA4, 0xE0, 0xB8, 0xA5, 0xE0, 0xB8, 0xA6,
                   0xE0, 0xB8, 0xA7, 0xE0, 0xB8, 0xA8, 0xE0, 0xB8, 0xA9,
                   0xE0, 0xB8, 0xAA, 0xE0, 0xB8, 0xAB, 0xE0, 0xB8, 0xAC,
                   0xE0, 0xB8, 0xAD, 0xE0, 0xB8, 0xAE, 0xE0, 0xB8, 0xAF,
                   0xE0, 0xB8, 0xB0, 0xE0, 0xB8, 0xB1, 0xE0, 0xB8, 0xB2,
                   0xE0, 0xB8, 0xB3, 0xE0, 0xB8, 0xB4, 0xE0, 0xB8, 0xB5,
                   0xE0, 0xB8, 0xB6, 0xE0, 0xB8, 0xB7, 0xE0, 0xB8, 0xB8,
                   0xE0, 0xB8, 0xB9, 0xE0, 0xB8, 0xBA, 0xE0, 0xB8, 0xBF,
                   0xE0, 0xB9, 0x80, 0xE0, 0xB9, 0x81, 0xE0, 0xB9, 0x82],

                  [0xE0, 0xB9, 0x83, 0xE0, 0xB9, 0x84, 0xE0, 0xB9, 0x85,
                   0xE0, 0xB9, 0x86, 0xE0, 0xB9, 0x87, 0xE0, 0xB9, 0x88,
                   0xE0, 0xB9, 0x89, 0xE0, 0xB9, 0x8A, 0xE0, 0xB9, 0x8B,
                   0xE0, 0xB9, 0x8C, 0xE0, 0xB9, 0x8D, 0xE0, 0xB9, 0x8E,
                   0xE0, 0xB9, 0x8F, 0xE0, 0xB9, 0x90, 0xE0, 0xB9, 0x91,
                   0xE0, 0xB9, 0x92, 0xE0, 0xB9, 0x93, 0xE0, 0xB9, 0x94,
                   0xE0, 0xB9, 0x95, 0xE0, 0xB9, 0x96, 0xE0, 0xB9, 0x97,
                   0xE0, 0xB9, 0x98, 0xE0, 0xB9, 0x99, 0xE0, 0xB9, 0x9A,
                   0xE0, 0xB9, 0x9B]];

  var expectedUTF16 = data.map(function(d) {
    return new Uint8Array(new Uint16Array(arrayFromString(d)).buffer);
  });

  // STREAMING TEST ONE: test streaming three valid strings with stream option
  // set to true for all three.
  testArrayOfStrings({encoding: "utf-8", array: [
    {input: data[0], stream: true, expected: expected[0]},
    {input: data[1], stream: true, expected: expected[1]},
    {input: data[2], stream: true, expected: expected[2]},
  ], msg: "streaming test one."});

  // STREAMING TEST TWO: test streaming valid strings with stream option
  // streaming option: false from constructor, string 1 stream: true,
  // string 2 stream: false, string 3 stream: false
  testArrayOfStrings({encoding: "utf-16", array: [
    {input: data[0], stream: true, expected: expectedUTF16[0]},
    {input: data[1], expected: expectedUTF16[1]},
    {input: data[2], expected: expectedUTF16[2]},
  ], msg: "streaming test two."});
}

function arrayFromString(s) {
  return s.split('').map(function(c){return String.charCodeAt(c)});
}

function testArrayOfStrings(test)
{
  var encoder;
  try {
    encoder = new TextEncoder(test.encoding);
  } catch (e) {
    assert_equals(e.name, test.error, test.msg);
    return;
  }
  assert_true(!test.error, test.msg);

  var array = test.array;
  for (var i = 0; i < array.length; i += 1) {
    var stream = array[i].stream ? {stream: true} : null;
    var view = encoder.encode(array[i].input, stream);

    var stringLen = view.length;
    var expected = array[i].expected;
    for (var j = 0; j < stringLen; j++) {
      if (view[j] !== expected[j]) {
        assert_equals(view[j], expected[j], msg + " Bytes do not match expected bytes.");
        return;
      }
    }
  }
}

function testEncoderGetEncoding()
{
  var labelEncodings = [
    {encoding: "utf-8", labels: ["unicode-1-1-utf-8", "utf-8", "utf8"]},
    {encoding: "utf-16le", labels: ["utf-16", "utf-16"]},
    {encoding: "utf-16be", labels: ["utf-16be"]},
  ];

  for (var le of labelEncodings) {
    for (var label of le.labels) {
      var encoder = new TextEncoder(label);
      assert_equals(encoder.encoding, le.encoding, label + " label encoding test.");
    }
  }
}
