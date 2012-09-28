/*
 * test_TextDecoderOptions.js
 * bug 764234 tests
*/

function runTextDecoderOptions()
{
  const data = [0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1,
                0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb,
                0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,
                0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
                0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
                0xda, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
                0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1,
                0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb];

  const expectedString = "\u00a0\u0e01\u0e02\u0e03\u0e04\u0e05\u0e06\u0e07"
                         + "\u0e08\u0e09\u0e0a\u0e0b\u0e0c\u0e0d\u0e0e\u0e0f"
                         + "\u0e10\u0e11\u0e12\u0e13\u0e14\u0e15\u0e16\u0e17"
                         + "\u0e18\u0e19\u0e1a\u0e1b\u0e1c\u0e1d\u0e1e\u0e1f"
                         + "\u0e20\u0e21\u0e22\u0e23\u0e24\u0e25\u0e26\u0e27"
                         + "\u0e28\u0e29\u0e2a\u0e2b\u0e2c\u0e2d\u0e2e\u0e2f"
                         + "\u0e30\u0e31\u0e32\u0e33\u0e34\u0e35\u0e36\u0e37"
                         + "\u0e38\u0e39\u0e3a\u0e3f\u0e40\u0e41\u0e42\u0e43"
                         + "\u0e44\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a\u0e4b"
                         + "\u0e4c\u0e4d\u0e4e\u0e4f\u0e50\u0e51\u0e52\u0e53"
                         + "\u0e54\u0e55\u0e56\u0e57\u0e58\u0e59\u0e5a\u0e5b";

  test(testDecoderGetEncoding, "testDecoderGetEncoding");
  test(testDecodeGreek, "testDecodeGreek");
  test(function() {
    testConstructorFatalOption(data, expectedString);
  }, "testConstructorFatalOption");
  test(function() {
    testConstructorEncodingOption(data, expectedString);
  }, "testConstructorEncodingOption");
  test(function() {
    testDecodeStreamOption(data, expectedString);
  }, "testDecodeStreamOption");
  test(testDecodeStreamCompositions, "testDecodeStreamCompositions");
  test(function() {
    testDecodeABVOption(data, expectedString);
  }, "testDecodeABVOption");
  test(testDecoderForThaiEncoding, "testDecoderForThaiEncoding");
}

/*
* function testConstructor()
*
* - This function tests the constructor optional arguments.
* - Stream option remains null for this test.
* - The stream option is passed to the decode function.
* - This function is not testing the decode function.
*
*/
function testConstructorFatalOption(data, expectedString)
{
  //invalid string to decode passed, fatal = false
  testCharset({fatal: false, encoding: "iso-8859-11", input: [], expected: "",
    msg: "constructor fatal option set to false test."});

  //invalid string to decode passed, fatal = true
  testCharset({fatal: true, encoding: "iso-8859-11", input: [], expected: "",
    msg: "constructor fatal option set to true test."});
}

function testConstructorEncodingOption(aData, aExpectedString)
{
  // valid encoding passed
  testCharset({encoding: "iso-8859-11", input: aData, expected: aExpectedString,
    msg: "decoder testing constructor valid encoding."});

  // invalid encoding passed
  testCharset({encoding: "asdfasdf", input: aData, error: "EncodingError",
    msg: "constructor encoding, invalid encoding test."});

  // passing spaces for encoding
  testCharset({encoding: "   ", input: aData, error: "EncodingError",
    msg: "constructor encoding, spaces encoding test."});

  // passing null for encoding
  testCharset({encoding: null, input: aData, error: "EncodingError",
    msg: "constructor encoding, \"null\" encoding test."});

  // empty encoding passed
  testCharset({encoding: "", input: aData, error: "EncodingError",
    msg: "constuctor encoding, empty encoding test."});

  // replacement character test
  aExpectedString = "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd"
                  + "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd";
  testCharset({encoding: "utf-8", input: aData, expected: aExpectedString,
    msg: "constuctor encoding, utf-8 test."});
}

/*
* function testDecodeStreamOption()
*
* - fatal remains null for the entire test
* - encoding remains as "iso-8859-11"
* - The stream option is modified for this test.
* - ArrayBufferView is modified for this test.
*/
function testDecodeStreamOption(data, expectedString)
{
  const streamData = [[0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
                      0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
                      0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,
                      0xb9, 0xba, 0xbb, 0xbc, 0xbd],
                      [0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,
                      0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
                      0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
                      0xd8, 0xd9, 0xda, 0xdf, 0xe0, 0xe1, 0xe2],
                      [0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
                      0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
                      0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb]];

  const expectedStringOne = "\u00a0\u0e01\u0e02\u0e03\u0e04\u0e05\u0e06\u0e07"
                            + "\u0e08\u0e09\u0e0a\u0e0b\u0e0c\u0e0d\u0e0e\u0e0f"
                            + "\u0e10\u0e11\u0e12\u0e13\u0e14\u0e15\u0e16\u0e17"
                            + "\u0e18\u0e19\u0e1a\u0e1b\u0e1c\u0e1d";
  const expectedStringTwo = "\u0e1e\u0e1f\u0e20\u0e21\u0e22\u0e23\u0e24\u0e25"
                            + "\u0e26\u0e27\u0e28\u0e29\u0e2a\u0e2b\u0e2c\u0e2d"
                            + "\u0e2e\u0e2f\u0e30\u0e31\u0e32\u0e33\u0e34\u0e35"
                            + "\u0e36\u0e37\u0e38\u0e39\u0e3a\u0e3f\u0e40\u0e41"
                            + "\u0e42";
  const expectedStringThree = "\u0e43\u0e44\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a"
                              + "\u0e4b\u0e4c\u0e4d\u0e4e\u0e4f\u0e50\u0e51"
                              + "\u0e52\u0e53\u0e54\u0e55\u0e56\u0e57\u0e58"
                              + "\u0e59\u0e5a\u0e5b";
  expectedString = [expectedStringOne, expectedStringTwo, expectedStringThree];

  // streaming test

  /* - the streaming is null
  *  - streaming is not set in the decode function
  */
  testCharset({encoding: "iso-8859-11", array: [
    {input: streamData[0], expected: expectedStringOne},
    {input: streamData[1], expected: expectedStringTwo},
    {input: streamData[2], expected: expectedStringThree},
  ], msg: "decode() stream test zero."});

  testCharset({encoding: "iso-8859-11", array: [
    {input: streamData[0], expected: expectedStringOne, stream: true},
    {input: streamData[1], expected: expectedStringTwo, stream: true},
    {input: streamData[2], expected: expectedStringThree, stream: true},
  ], msg: "decode() stream test one."});

  testCharset({encoding: "iso-8859-11", array: [
    {input: streamData[0], expected: expectedStringOne, stream: true},
    {input: streamData[1], expected: expectedStringTwo},
    {input: streamData[2], expected: expectedStringThree},
  ], msg: "decode() stream test two."});

  testCharset({encoding: "utf-8", array: [
    {input: [0xC2], expected: "\uFFFD"},
    {input: [0x80], expected: "\uFFFD"},
  ], msg: "decode() stream test utf-8."});

  testCharset({encoding: "utf-8", fatal: true, array: [
    {input: [0xC2], error: "EncodingError"},
    {input: [0x80], error: "EncodingError"},
  ], msg: "decode() stream test utf-8 fatal."});
}

function testDecodeStreamCompositions() {
  var tests = [
    {encoding: "utf-8", input: [0xC2,0x80], expected: ["","\x80"]},
    {encoding: "utf-8", input: [0xEF,0xBB,0xBF,0xC2,0x80], expected: ["","","","","\x80"]},
    {encoding: "utf-16", input: [0x01,0x00], expected: ["","\x01"]},
    {encoding: "utf-16", input: [0x01,0x00,0x03,0x02], expected: ["","\x01","","\u0203"]},
    {encoding: "utf-16", input: [0xFF,0xFE], expected: ["",""]},
    {encoding: "utf-16", input: [0xFF,0xFE,0x01,0x00], expected: ["","","","\x01"]},
    {encoding: "utf-16", input: [0xFF,0xFE,0xFF,0xFE], expected: ["","","","\uFEFF"]},
    {encoding: "utf-16", input: [0xFF,0xFE,0xFE,0xFF], expected: ["","","","\uFFFE"]},
    {encoding: "utf-16", input: [0xFE,0xFF], expected: ["",""]},
    {encoding: "utf-16", input: [0xFE,0xFF,0x01,0x00], expected: ["","","","\u0100"]},
    {encoding: "utf-16", input: [0xFE,0xFF,0xFF,0xFE], expected: ["","","","\uFFFE"]},
    {encoding: "utf-16", input: [0xFE,0xFF,0xFE,0xFF], expected: ["","","","\uFEFF"]},
    {encoding: "utf-16le", input: [0x01,0x00], expected: ["","\x01"]},
    {encoding: "utf-16le", input: [0x01,0x00,0x03,0x02], expected: ["","\x01","","\u0203"]},
    {encoding: "utf-16le", input: [0xFF,0xFE,0x01,0x00], expected: ["","","","\x01"]},
    {encoding: "utf-16le", input: [0xFE,0xFF,0x01,0x00], expected: ["","\uFFFE","","\x01"]},
    {encoding: "utf-16be", input: [0x01,0x00], expected: ["","\u0100"]},
    {encoding: "utf-16be", input: [0x01,0x00,0x03,0x02], expected: ["","\u0100","","\u0302"]},
    {encoding: "utf-16be", input: [0xFF,0xFE,0x01,0x00], expected: ["","\uFFFE","","\u0100"]},
    {encoding: "utf-16be", input: [0xFE,0xFF,0x01,0x00], expected: ["","","","\u0100"]},
    {encoding: "shift_jis", input: [0x81,0x40], expected: ["","\u3000"]},
  ];
  tests.forEach(function(t) {
    (function generateCompositions(a, n) {
      a.push(n);
      var l = a.length - 1;
      var array=[];
      for (var i = 0, o = 0; i <= l; i++) {
        array.push({
          input: t.input.slice(o, o+a[i]),
          expected: t.expected.slice(o, o+=a[i]).join(""),
          stream: i < l
        });
      }
      testCharset({encoding: t.encoding, array: array,
        msg: "decode() stream test " + t.encoding + " " + a.join("-") + "."});
      while (a[l] > 1) {
        a[l]--;
        generateCompositions(a.slice(0), n - a[l]);
      }
    })([], t.input.length);
  });
}

/*
* function testDecodeABVOption()
*
* - ABV for ArrayBufferView
* - fatal remains null for the entire test
* - encoding remains as "iso-8859-11"
* - The stream option is modified for this test.
* - ArrayBufferView is modified for this test.
*/
function testDecodeABVOption(data, expectedString)
{
  // valid data
  testCharset({encoding: "iso-8859-11", input: data, expected: expectedString,
    msg: "decode test ABV valid data."});

  // invalid empty data
  testCharset({encoding: "iso-8859-11", input: [], expected: "",
    msg: "decode test ABV empty data."});

  // spaces
  testCharset({encoding: "iso-8859-11", input: ["\u0020\u0020"], expected: "\0",
    msg: "text decoding ABV string test."});

  testCharset({encoding: "iso-8859-11", input: [""], expected: "\0",
    msg: "text decoding ABV empty string test."});

  // null for Array Buffer
  testCharset({encoding: "iso-8859-11", input: null, expected: "",
    msg: "text decoding ABV null test."});
}

function testDecodeGreek()
{
  var data = [0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
              0xa9, 0xaa, 0xab, 0xac, 0xad, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4,
              0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
              0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
              0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd3, 0xd4, 0xd5, 0xd6,
              0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1,
              0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec,
              0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
              0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe];

  var expectedString = "\u00a0\u2018\u2019\u00a3\u20ac\u20af\u00a6\u00a7\u00a8"
                       + "\u00a9\u037a\u00ab\u00ac\u00ad\u2015\u00b0\u00b1"
                       + "\u00b2\u00b3\u0384\u0385\u0386\u00b7\u0388\u0389"
                       + "\u038a\u00bb\u038c\u00bd\u038e\u038f\u0390\u0391"
                       + "\u0392\u0393\u0394\u0395\u0396\u0397\u0398\u0399"
                       + "\u039a\u039b\u039c\u039d\u039e\u039f\u03a0\u03a1"
                       + "\u03a3\u03a4\u03a5\u03a6\u03a7\u03a8\u03a9\u03aa"
                       + "\u03ab\u03ac\u03ad\u03ae\u03af\u03b0\u03b1\u03b2"
                       + "\u03b3\u03b4\u03b5\u03b6\u03b7\u03b8\u03b9\u03ba"
                       + "\u03bb\u03bc\u03bd\u03be\u03bf\u03c0\u03c1\u03c2"
                       + "\u03c3\u03c4\u03c5\u03c6\u03c7\u03c8\u03c9\u03ca"
                       + "\u03cb\u03cc\u03cd\u03ce";

  testCharset({encoding: "greek", input: data, expected: expectedString,
    msg: "decode greek test."});
}

function testDecoderForThaiEncoding()
{
  // TEST One
  const data = [0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb];

  const expectedString = "\u00a0\u0e01\u0e02\u0e03\u0e04\u0e05\u0e06\u0e07\u0e08\u0e09\u0e0a\u0e0b\u0e0c\u0e0d\u0e0e\u0e0f\u0e10\u0e11\u0e12\u0e13\u0e14\u0e15\u0e16\u0e17\u0e18\u0e19\u0e1a\u0e1b\u0e1c\u0e1d\u0e1e\u0e1f\u0e20\u0e21\u0e22\u0e23\u0e24\u0e25\u0e26\u0e27\u0e28\u0e29\u0e2a\u0e2b\u0e2c\u0e2d\u0e2e\u0e2f\u0e30\u0e31\u0e32\u0e33\u0e34\u0e35\u0e36\u0e37\u0e38\u0e39\u0e3a\u0e3f\u0e40\u0e41\u0e42\u0e43\u0e44\u0e45\u0e46\u0e47\u0e48\u0e49\u0e4a\u0e4b\u0e4c\u0e4d\u0e4e\u0e4f\u0e50\u0e51\u0e52\u0e53\u0e54\u0e55\u0e56\u0e57\u0e58\u0e59\u0e5a\u0e5b";

  const aliases = [ "ISO-8859-11", "iso-8859-11", "iso8859-11", "iso885911" ];

  testCharset({encoding: "iso-8859-11", input: data, expected: expectedString,
    msg: "decoder testing valid ISO-8859-11 encoding."});
}

function testDecoderGetEncoding()
{
  var labelEncodings = [
    {label: "utf-16", encoding: "utf-16"},
    {label: "utf-16le", encoding: "utf-16"},
    {label: "euc-kr", encoding: "euc-kr"},
    {label: "x-windows-949", error: "EncodingError"},
  ];

  labelEncodings.forEach(function(le){
    try {
      var decoder = TextDecoder(le.label);
      if (le.encoding) {
        assert_equals(decoder.encoding, le.encoding, le.label + " label encoding test.");
      } else {
        assert_unreached(le.label + " label encoding unsupported test should throw " + le.error);
      }
    } catch (e) {
      assert_equals(e.name, le.error, le.label + " label encoding unsupported test.");
    }
  });
}

function testCharset(test)
{
  try {
    var fatal = test.fatal ? {fatal: test.fatal} : null;
    var decoder = TextDecoder(test.encoding, fatal);
  } catch (e) {
    assert_equals(e.name, test.error, test.msg + " error thrown from the constructor.");
    return;
  }

  var array = test.array || [test];
  var num_strings = array.length;
  for (var i = 0; i < num_strings; i++) {
    var decodeView = array[i].input !== null ? new Uint8Array(array[i].input) : null;
    var stream = array[i].stream ? {stream: array[i].stream} : null;
    var outText;
    try {
      outText = decoder.decode(decodeView, stream);
    } catch (e) {
      assert_equals(e.name, array[i].error, test.msg + " error thrown from decode().");
      return;
    }

    var expected = array[i].expected;
    if (outText !== expected) {
      assert_equals(escape(outText), escape(expected), test.msg + " Code points do not match expected code points.");
      break;
    }
  }
  assert_true(!test.error, test.msg);
}
