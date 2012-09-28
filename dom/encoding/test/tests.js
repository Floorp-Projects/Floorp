// NOTE: relies on http://docs.jquery.com/QUnit

function arrayEqual(a, b, msg) {
  // deepEqual() is too picky
  equal(a.length, b.length, msg + " - array length");
  for (var i = 0; i < a.length; i += 1) {
    // equal() is too slow to call each time
    if (a[i] !== b[i]) { equal(a[i], b[i], msg + " - first array index: " + String(i) ); return; }
  }
  ok(true, msg);
}


function testEncodeDecode(encoding, min, max) {
  function cpname(n) {
    return 'U+' + ((n <= 0xFFFF) ?
                   ('0000' + n.toString(16).toUpperCase()).slice(-4) :
                   n.toString(16).toUpperCase());
  }

  test(
    encoding + " - Encode/Decode Range " + cpname(min) + " - " + cpname(max),
    function() {
      var string, i, j, BATCH_SIZE = 0x1000;
      for (i = min; i < max; i += BATCH_SIZE) {
        string = '';
        for (j = i; j < i + BATCH_SIZE && j < max; j += 1) {
          if (0xd800 <= j && j <= 0xdfff) {
            // surrogate half
            continue;
          } else if (j > 0xffff) {
            // outside BMP - encode as surrogate pair
            string += String.fromCharCode(
	      0xd800 + ((j >> 10) & 0x3ff),
	      0xdc00 + (j & 0x3ff));
          } else {
            string += String.fromCharCode(i);
          }
        }
        var encoded = TextEncoder(encoding).encode(string);
        var decoded = TextDecoder(encoding).decode(encoded);
        equal(string, decoded, 'Round trip ' + cpname(i) + " - " + cpname(j));
      }
    });
}

testEncodeDecode('UTF-8', 0, 0x10FFFF);
testEncodeDecode('UTF-16LE', 0, 0x10FFFF);
testEncodeDecode('UTF-16BE', 0, 0x10FFFF);
testEncodeDecode('windows-1252', 0, 0xFF);


// Inspired by:
// http://ecmanaut.blogspot.com/2006/07/encoding-decoding-utf8-in-javascript.html
function encode_utf8(string) {
  var utf8 = unescape(encodeURIComponent(string));
  var octets = [], i;
  for (i = 0; i < utf8.length; i += 1) {
    octets.push(utf8.charCodeAt(i));
  }
  return octets;
}

function decode_utf8(octets) {
  var utf8 = String.fromCharCode.apply(null, octets);
  return decodeURIComponent(escape(utf8));
}

test(
  "UTF-8 encoding (compare against unescape/encodeURIComponent)",
  function() {
    expect(544);

    var actual, expected, str, i, j, BATCH_SIZE = 0x1000;

    for (i = 0; i < 0x10FFFF; i += BATCH_SIZE) {
      str = '';
      for (j = i; j < i + BATCH_SIZE; j += 1) {
        if (0xd800 <= j && j <= 0xdfff) {
          // surrogate half
          continue;
        } else if (j > 0xffff) {
          // outside BMP - encode as surrogate pair
          str += String.fromCharCode(
	    0xd800 + ((j >> 10) & 0x3ff),
	    0xdc00 + (j & 0x3ff));
        } else {
          str += String.fromCharCode(i);
        }
      }
      expected = encode_utf8(str);

      actual = TextEncoder('UTF-8').encode(str);
      arrayEqual(actual, expected, 'expected equal encodings');
    }
  });

test(
  "UTF-8 decoding (compare against decodeURIComponent/escape)",
  function() {
    expect(272);

    var encoded, actual, expected, str, i, j, BATCH_SIZE = 0x1000;

    for (i = 0; i < 0x10FFFF; i += BATCH_SIZE) {
      str = '';
      for (j = i; j < i + BATCH_SIZE; j += 1) {
        if (0xd800 <= j && j <= 0xdfff) {
          // surrogate half
          continue;
        } else if (j > 0xffff) {
          // outside BMP - encode as surrogate pair
          str += String.fromCharCode(
	    0xd800 + ((j >> 10) & 0x3ff),
	    0xdc00 + (j & 0x3ff));
        } else {
          str += String.fromCharCode(i);
        }
      }

      encoded = encode_utf8(str);

      expected = decode_utf8(encoded);
      actual = TextDecoder('UTF-8').decode(new Uint8Array(encoded));

      equal(actual, expected, 'expected equal decodings');
    }
  });

function testEncodeDecodeSample(encoding, string, expected) {
  test(
    encoding + " - Encode/Decode - reference sample",
    function() {
      expect(3);

      var encoded = TextEncoder(encoding).encode(string);
      arrayEqual(encoded, expected, 'expected equal encodings ' + encoding);

      var decoded = TextDecoder(encoding).decode(new Uint8Array(expected));
      equal(decoded, string, 'expected equal decodings ' + encoding);
    });
}

testEncodeDecodeSample(
  "windows-1252",
  "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7f\u20ac\x81\u201a\u0192\u201e\u2026\u2020\u2021\u02c6\u2030\u0160\u2039\u0152\x8d\u017d\x8f\x90\u2018\u2019\u201c\u201d\u2022\u2013\u2014\u02dc\u2122\u0161\u203a\u0153\x9d\u017e\u0178\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff",
  (function() { var i = 0, a = []; while (i < 256) { a.push(i++); } return a; }())
);

testEncodeDecodeSample(
  "utf-8",
  "z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD", // z, cent, CJK water, G-Clef, Private-use character
  [0x7A, 0xC2, 0xA2, 0xE6, 0xB0, 0xB4, 0xF0, 0x9D, 0x84, 0x9E, 0xF4, 0x8F, 0xBF, 0xBD]
);
testEncodeDecodeSample(
  "utf-16le",
  "z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD", // z, cent, CJK water, G-Clef, Private-use character
  [0x7A, 0x00, 0xA2, 0x00, 0x34, 0x6C, 0x34, 0xD8, 0x1E, 0xDD, 0xFF, 0xDB, 0xFD, 0xDF]
);
testEncodeDecodeSample(
  "utf-16be",
  "z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD", // z, cent, CJK water, G-Clef, Private-use character
  [0x00, 0x7A, 0x00, 0xA2, 0x6C, 0x34, 0xD8, 0x34, 0xDD, 0x1E, 0xDB, 0xFF, 0xDF, 0xFD]
);
testEncodeDecodeSample(
  "utf-16",
  "z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD", // z, cent, CJK water, G-Clef, Private-use character
  [0x7A, 0x00, 0xA2, 0x00, 0x34, 0x6C, 0x34, 0xD8, 0x1E, 0xDD, 0xFF, 0xDB, 0xFD, 0xDF]
);

test(
  "bad data",
  function() {
    expect(5);

    var badStrings = [
      { input: '\ud800', expected: '\ufffd' }, // Surrogate half
      { input: '\udc00', expected: '\ufffd' }, // Surrogate half
      { input: 'abc\ud800def', expected: 'abc\ufffddef' }, // Surrogate half
      { input: 'abc\udc00def', expected: 'abc\ufffddef' }, // Surrogate half
      { input: '\udc00\ud800', expected: '\ufffd\ufffd' } // Wrong order
    ];

    badStrings.forEach(
      function(t) {
        var encoded = TextEncoder('utf-8').encode(t.input);
        var decoded = TextDecoder('utf-8').decode(encoded);
        equal(t.expected, decoded);
      });
  });

test(
  "fatal flag",
  function() {
    expect(14);

    var bad = [
      { encoding: 'utf-8', input: [0xC0] }, // ends early
      { encoding: 'utf-8', input: [0xC0, 0x00] }, // invalid trail
      { encoding: 'utf-8', input: [0xC0, 0xC0] }, // invalid trail
      { encoding: 'utf-8', input: [0xE0] }, // ends early
      { encoding: 'utf-8', input: [0xE0, 0x00] }, // invalid trail
      { encoding: 'utf-8', input: [0xE0, 0xC0] }, // invalid trail
      { encoding: 'utf-8', input: [0xE0, 0x80, 0x00] }, // invalid trail
      { encoding: 'utf-8', input: [0xE0, 0x80, 0xC0] }, // invalid trail
      { encoding: 'utf-8', input: [0xFC, 0x80, 0x80, 0x80, 0x80, 0x80] }, // > 0x10FFFF
      { encoding: 'utf-16', input: [0x00] }, // truncated code unit
      { encoding: 'utf-16', input: [0x00, 0xd8] }, // surrogate half
      { encoding: 'utf-16', input: [0x00, 0xd8, 0x00, 0x00] }, // surrogate half
      { encoding: 'utf-16', input: [0x00, 0xdc, 0x00, 0x00] }, // trail surrogate
      { encoding: 'utf-16', input: [0x00, 0xdc, 0x00, 0xd8] }  // swapped surrogates
      // TODO: Single byte encoding cases
    ];

    bad.forEach(
      function(t) {
        raises(function () { TextDecoder('utf-8').encode(t.input, {fatal: true}); });
      });
  });

test(
  "Encoding names are case insensitive", function() {
    var encodings = [
      { encoding: 'UTF-8', string: 'z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD' },
      { encoding: 'UTF-16', string: 'z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD' },
      { encoding: 'UTF-16LE', string: 'z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD' },
      { encoding: 'UTF-16BE', string: 'z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD' },
      { encoding: 'ASCII', string: 'ABCabc123!@#' },
      { encoding: 'ISO-8859-1', string: 'ABCabc123!@#\xA2' }
    ];

    encodings.forEach(
      function(test) {
        var lower = test.encoding.toLowerCase();
        var upper = test.encoding.toUpperCase();
        equal(
          TextDecoder(lower).decode(TextEncoder(lower).encode(test.string)),
          TextDecoder(upper).decode(TextEncoder(upper).encode(test.string))
        );
      });
  });

test(
  "Byte-order marks",
  function() {
    //expect(9);

    var utf8 = [0xEF, 0xBB, 0xBF, 0x7A, 0xC2, 0xA2, 0xE6, 0xB0, 0xB4, 0xF0, 0x9D, 0x84, 0x9E, 0xF4, 0x8F, 0xBF, 0xBD];
    var utf16le = [0xff, 0xfe, 0x7A, 0x00, 0xA2, 0x00, 0x34, 0x6C, 0x34, 0xD8, 0x1E, 0xDD, 0xFF, 0xDB, 0xFD, 0xDF];
    var utf16be = [0xfe, 0xff, 0x00, 0x7A, 0x00, 0xA2, 0x6C, 0x34, 0xD8, 0x34, 0xDD, 0x1E, 0xDB, 0xFF, 0xDF, 0xFD];

    var string = "z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD"; // z, cent, CJK water, G-Clef, Private-use character

    // Basic cases
    equal(TextDecoder('utf-8').decode(new Uint8Array(utf8)), string);
    equal(TextDecoder('utf-16le').decode(new Uint8Array(utf16le)), string);
    equal(TextDecoder('utf-16be').decode(new Uint8Array(utf16be)), string);

    /*
    // TODO: New API?
    // Verify that BOM wins
    equal(stringEncoding.decode(new Uint8Array(utf8), 'utf-16le'), string);
    equal(stringEncoding.decode(new Uint8Array(utf8), 'utf-16be'), string);
    equal(stringEncoding.decode(new Uint8Array(utf16le), 'utf-8'), string);
    equal(stringEncoding.decode(new Uint8Array(utf16le), 'utf-16be'), string);
    equal(stringEncoding.decode(new Uint8Array(utf16be), 'utf-8'), string);
    equal(stringEncoding.decode(new Uint8Array(utf16be), 'utf-16le'), string);
    */
  });

test(
  "Encoding names",
  function () {
    equal(TextEncoder("utf-8").encoding, "utf-8"); // canonical case
    equal(TextEncoder("UTF-16").encoding, "utf-16"); // canonical case and name
    equal(TextEncoder("UTF-16BE").encoding, "utf-16be"); // canonical case and name
    equal(TextEncoder("iso8859-1").encoding, "windows-1252"); // canonical case and name
    equal(TextEncoder("iso-8859-1").encoding, "windows-1252"); // canonical case and name

  });

test(
  "Streaming Decode",
  function () {
    ["utf-8", "utf-16le", "utf-16be"].forEach(function (encoding) {
      var string = "\x00123ABCabc\x80\xFF\u0100\u1000\uFFFD\uD800\uDC00\uDBFF\uDFFF";
      var encoded = TextEncoder(encoding).encode(string);

      for (var len = 1; len <= 5; ++len) {
        var out = "", decoder = TextDecoder(encoding);
        for (var i = 0; i < encoded.length; i += len) {
          var sub = [];
          for (var j = i; j < encoded.length && j < i + len; ++j) {
            sub.push(encoded[j]);
          }
          out += decoder.decode(new Uint8Array(sub), {stream: true});
        }
        out += decoder.decode();
        equal(out, string, "streaming decode " + encoding);
      }
    });
  });

test(
  "Shift_JIS Decode",
  function () {
    var jis = [0x82, 0xC9, 0x82, 0xD9, 0x82, 0xF1];
    var expected = "\u306B\u307B\u3093"; // Nihon
    equal(TextDecoder("shift_jis").decode(new Uint8Array(jis)), expected);
  });

test(
  "Supersets of ASCII decode ASCII correctly",
  38,
  function () {
    var encodings = ["utf-8", "ibm864", "ibm866", "iso-8859-2", "iso-8859-3", "iso-8859-4", "iso-8859-5", "iso-8859-6", "iso-8859-7", "iso-8859-8", "iso-8859-10", "iso-8859-13", "iso-8859-14", "iso-8859-15", "iso-8859-16", "koi8-r", "koi8-u", "macintosh", "windows-874", "windows-1250", "windows-1251", "windows-1252", "windows-1253", "windows-1254", "windows-1255", "windows-1256", "windows-1257", "windows-1258", "x-mac-cyrillic", "gbk", "gb18030", "hz-gb-2312", "big5", "euc-jp", "iso-2022-jp", "shift_jis", "euc-kr", "iso-2022-kr"];

    encodings.forEach(function (encoding) {
      var string = '', bytes = [];
      for (var i = 0; i < 128; ++i) {

        // Encodings that have escape codes in 0x00-0x7F
        if (encoding === "hz-gb-2312" && i === 0x7E)
          continue;
        if (encoding === "iso-2022-jp" && i === 0x1B)
          continue;
        if (encoding === "iso-2022-kr" && (i === 0x0E || i === 0x0F || i === 0x1B))
          continue;

        string += String.fromCharCode(i);
        bytes.push(i);
      }
      var ascii_encoded = TextEncoder('ascii').encode(string);
      equal(TextDecoder(encoding).decode(ascii_encoded), string, encoding);
      //arrayEqual(TextEncoder(encoding).encode(string), bytes, encoding);
    });
  });
