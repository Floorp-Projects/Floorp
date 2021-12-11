// NOTE: Requires testharness.js
// http://www.w3.org/2008/webapps/wiki/Harness

test(function() {
  var badStrings = [
    { input: "\ud800", expected: "\ufffd" }, // Surrogate half
    { input: "\udc00", expected: "\ufffd" }, // Surrogate half
    { input: "abc\ud800def", expected: "abc\ufffddef" }, // Surrogate half
    { input: "abc\udc00def", expected: "abc\ufffddef" }, // Surrogate half
    { input: "\udc00\ud800", expected: "\ufffd\ufffd" }, // Wrong order
  ];

  badStrings.forEach(function(t) {
    var encoded = new TextEncoder("utf-8").encode(t.input);
    var decoded = new TextDecoder("utf-8").decode(encoded);
    assert_equals(t.expected, decoded);
  });
}, "bad data");

test(function() {
  var bad = [
    { encoding: "utf-8", input: [0xc0] }, // ends early
    { encoding: "utf-8", input: [0xc0, 0x00] }, // invalid trail
    { encoding: "utf-8", input: [0xc0, 0xc0] }, // invalid trail
    { encoding: "utf-8", input: [0xe0] }, // ends early
    { encoding: "utf-8", input: [0xe0, 0x00] }, // invalid trail
    { encoding: "utf-8", input: [0xe0, 0xc0] }, // invalid trail
    { encoding: "utf-8", input: [0xe0, 0x80, 0x00] }, // invalid trail
    { encoding: "utf-8", input: [0xe0, 0x80, 0xc0] }, // invalid trail
    { encoding: "utf-8", input: [0xfc, 0x80, 0x80, 0x80, 0x80, 0x80] }, // > 0x10FFFF
    { encoding: "utf-16le", input: [0x00] }, // truncated code unit
    { encoding: "utf-16le", input: [0x00, 0xd8] }, // surrogate half
    { encoding: "utf-16le", input: [0x00, 0xd8, 0x00, 0x00] }, // surrogate half
    { encoding: "utf-16le", input: [0x00, 0xdc, 0x00, 0x00] }, // trail surrogate
    { encoding: "utf-16le", input: [0x00, 0xdc, 0x00, 0xd8] }, // swapped surrogates
    // TODO: Single byte encoding cases
  ];

  bad.forEach(function(t) {
    assert_throws({ name: "TypeError" }, function() {
      new TextDecoder(t.encoding, { fatal: true }).decode(
        new Uint8Array(t.input)
      );
    });
  });
}, "fatal flag");

test(function() {
  var encodings = [
    { label: "utf-8", encoding: "utf-8" },
    { label: "utf-16", encoding: "utf-16le" },
    { label: "utf-16le", encoding: "utf-16le" },
    { label: "utf-16be", encoding: "utf-16be" },
    { label: "ascii", encoding: "windows-1252" },
    { label: "iso-8859-1", encoding: "windows-1252" },
  ];

  encodings.forEach(function(test) {
    assert_equals(
      new TextDecoder(test.label.toLowerCase()).encoding,
      test.encoding
    );
    assert_equals(
      new TextDecoder(test.label.toUpperCase()).encoding,
      test.encoding
    );
  });
}, "Encoding names are case insensitive");

test(function() {
  var utf8_bom = [0xef, 0xbb, 0xbf];
  var utf8 = [
    0x7a,
    0xc2,
    0xa2,
    0xe6,
    0xb0,
    0xb4,
    0xf0,
    0x9d,
    0x84,
    0x9e,
    0xf4,
    0x8f,
    0xbf,
    0xbd,
  ];

  var utf16le_bom = [0xff, 0xfe];
  var utf16le = [
    0x7a,
    0x00,
    0xa2,
    0x00,
    0x34,
    0x6c,
    0x34,
    0xd8,
    0x1e,
    0xdd,
    0xff,
    0xdb,
    0xfd,
    0xdf,
  ];

  var utf16be_bom = [0xfe, 0xff];
  var utf16be = [
    0x00,
    0x7a,
    0x00,
    0xa2,
    0x6c,
    0x34,
    0xd8,
    0x34,
    0xdd,
    0x1e,
    0xdb,
    0xff,
    0xdf,
    0xfd,
  ];

  var string = "z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD"; // z, cent, CJK water, G-Clef, Private-use character

  // missing BOMs
  assert_equals(new TextDecoder("utf-8").decode(new Uint8Array(utf8)), string);
  assert_equals(
    new TextDecoder("utf-16le").decode(new Uint8Array(utf16le)),
    string
  );
  assert_equals(
    new TextDecoder("utf-16be").decode(new Uint8Array(utf16be)),
    string
  );

  // matching BOMs
  assert_equals(
    new TextDecoder("utf-8").decode(new Uint8Array(utf8_bom.concat(utf8))),
    string
  );
  assert_equals(
    new TextDecoder("utf-16le").decode(
      new Uint8Array(utf16le_bom.concat(utf16le))
    ),
    string
  );
  assert_equals(
    new TextDecoder("utf-16be").decode(
      new Uint8Array(utf16be_bom.concat(utf16be))
    ),
    string
  );

  // matching BOMs split
  var decoder8 = new TextDecoder("utf-8");
  assert_equals(
    decoder8.decode(new Uint8Array(utf8_bom.slice(0, 1)), { stream: true }),
    ""
  );
  assert_equals(
    decoder8.decode(new Uint8Array(utf8_bom.slice(1).concat(utf8))),
    string
  );
  assert_equals(
    decoder8.decode(new Uint8Array(utf8_bom.slice(0, 2)), { stream: true }),
    ""
  );
  assert_equals(
    decoder8.decode(new Uint8Array(utf8_bom.slice(2).concat(utf8))),
    string
  );
  var decoder16le = new TextDecoder("utf-16le");
  assert_equals(
    decoder16le.decode(new Uint8Array(utf16le_bom.slice(0, 1)), {
      stream: true,
    }),
    ""
  );
  assert_equals(
    decoder16le.decode(new Uint8Array(utf16le_bom.slice(1).concat(utf16le))),
    string
  );
  var decoder16be = new TextDecoder("utf-16be");
  assert_equals(
    decoder16be.decode(new Uint8Array(utf16be_bom.slice(0, 1)), {
      stream: true,
    }),
    ""
  );
  assert_equals(
    decoder16be.decode(new Uint8Array(utf16be_bom.slice(1).concat(utf16be))),
    string
  );

  // mismatching BOMs
  assert_not_equals(
    new TextDecoder("utf-8").decode(new Uint8Array(utf16le_bom.concat(utf8))),
    string
  );
  assert_not_equals(
    new TextDecoder("utf-8").decode(new Uint8Array(utf16be_bom.concat(utf8))),
    string
  );
  assert_not_equals(
    new TextDecoder("utf-16le").decode(
      new Uint8Array(utf8_bom.concat(utf16le))
    ),
    string
  );
  assert_not_equals(
    new TextDecoder("utf-16le").decode(
      new Uint8Array(utf16be_bom.concat(utf16le))
    ),
    string
  );
  assert_not_equals(
    new TextDecoder("utf-16be").decode(
      new Uint8Array(utf8_bom.concat(utf16be))
    ),
    string
  );
  assert_not_equals(
    new TextDecoder("utf-16be").decode(
      new Uint8Array(utf16le_bom.concat(utf16be))
    ),
    string
  );
}, "Byte-order marks");

test(function() {
  assert_equals(new TextDecoder("utf-8").encoding, "utf-8"); // canonical case
  assert_equals(new TextDecoder("UTF-16").encoding, "utf-16le"); // canonical case and name
  assert_equals(new TextDecoder("UTF-16BE").encoding, "utf-16be"); // canonical case and name
  assert_equals(new TextDecoder("iso8859-1").encoding, "windows-1252"); // canonical case and name
  assert_equals(new TextDecoder("iso-8859-1").encoding, "windows-1252"); // canonical case and name
}, "Encoding names");

test(function() {
  ["utf-8", "utf-16le", "utf-16be"].forEach(function(encoding) {
    var string =
      "\x00123ABCabc\x80\xFF\u0100\u1000\uFFFD\uD800\uDC00\uDBFF\uDFFF";
    var octets = {
      "utf-16le": [
        0x00,
        0x00,
        0x31,
        0x00,
        0x32,
        0x00,
        0x33,
        0x00,
        0x41,
        0x00,
        0x42,
        0x00,
        0x43,
        0x00,
        0x61,
        0x00,
        0x62,
        0x00,
        0x63,
        0x00,
        0x80,
        0x00,
        0xff,
        0x00,
        0x00,
        0x01,
        0x00,
        0x10,
        0xfd,
        0xff,
        0x00,
        0xd8,
        0x00,
        0xdc,
        0xff,
        0xdb,
        0xff,
        0xdf,
      ],
      "utf-16be": [
        0x00,
        0x00,
        0x00,
        0x31,
        0x00,
        0x32,
        0x00,
        0x33,
        0x00,
        0x41,
        0x00,
        0x42,
        0x00,
        0x43,
        0x00,
        0x61,
        0x00,
        0x62,
        0x00,
        0x63,
        0x00,
        0x80,
        0x00,
        0xff,
        0x01,
        0x00,
        0x10,
        0x00,
        0xff,
        0xfd,
        0xd8,
        0x00,
        0xdc,
        0x00,
        0xdb,
        0xff,
        0xdf,
        0xff,
      ],
    };
    var encoded = octets[encoding] || new TextEncoder(encoding).encode(string);

    for (var len = 1; len <= 5; ++len) {
      var out = "",
        decoder = new TextDecoder(encoding);
      for (var i = 0; i < encoded.length; i += len) {
        var sub = [];
        for (var j = i; j < encoded.length && j < i + len; ++j) {
          sub.push(encoded[j]);
        }
        out += decoder.decode(new Uint8Array(sub), { stream: true });
      }
      out += decoder.decode();
      assert_equals(out, string, "streaming decode " + encoding);
    }
  });
}, "Streaming Decode");

test(function() {
  var jis = [0x82, 0xc9, 0x82, 0xd9, 0x82, 0xf1];
  var expected = "\u306B\u307B\u3093"; // Nihon
  assert_equals(
    new TextDecoder("shift_jis").decode(new Uint8Array(jis)),
    expected
  );
}, "Shift_JIS Decode");

test(function() {
  var encodings = [
    "utf-8",
    "ibm866",
    "iso-8859-2",
    "iso-8859-3",
    "iso-8859-4",
    "iso-8859-5",
    "iso-8859-6",
    "iso-8859-7",
    "iso-8859-8",
    "iso-8859-8-i",
    "iso-8859-10",
    "iso-8859-13",
    "iso-8859-14",
    "iso-8859-15",
    "iso-8859-16",
    "koi8-r",
    "koi8-u",
    "macintosh",
    "windows-874",
    "windows-1250",
    "windows-1251",
    "windows-1252",
    "windows-1253",
    "windows-1254",
    "windows-1255",
    "windows-1256",
    "windows-1257",
    "windows-1258",
    "x-mac-cyrillic",
    "gbk",
    "gb18030",
    "big5",
    "euc-jp",
    "iso-2022-jp",
    "shift_jis",
    "euc-kr",
    "x-user-defined",
  ];

  encodings.forEach(function(encoding) {
    var string = "",
      bytes = [];
    for (var i = 0; i < 128; ++i) {
      // Encodings that have escape codes in 0x00-0x7F
      if (
        encoding === "iso-2022-jp" &&
        (i === 0x1b || i === 0xe || i === 0xf)
      ) {
        continue;
      }

      string += String.fromCharCode(i);
      bytes.push(i);
    }
    var ascii_encoded = new TextEncoder("utf-8").encode(string);
    assert_equals(
      new TextDecoder(encoding).decode(ascii_encoded),
      string,
      encoding
    );
    //assert_array_equals(new TextEncoder(encoding).encode(string), bytes, encoding);
  });
}, "Supersets of ASCII decode ASCII correctly");

test(function() {
  assert_throws({ name: "TypeError" }, function() {
    new TextDecoder("utf-8", { fatal: true }).decode(new Uint8Array([0xff]));
  });
  // This should not hang:
  new TextDecoder("utf-8").decode(new Uint8Array([0xff]));

  assert_throws({ name: "TypeError" }, function() {
    new TextDecoder("utf-16", { fatal: true }).decode(new Uint8Array([0x00]));
  });
  // This should not hang:
  new TextDecoder("utf-16").decode(new Uint8Array([0x00]));

  assert_throws({ name: "TypeError" }, function() {
    new TextDecoder("utf-16be", { fatal: true }).decode(new Uint8Array([0x00]));
  });
  // This should not hang:
  new TextDecoder("utf-16be").decode(new Uint8Array([0x00]));
}, "Non-fatal errors at EOF");

test(function() {
  var encodings = [
    "utf-8",
    "ibm866",
    "iso-8859-2",
    "iso-8859-3",
    "iso-8859-4",
    "iso-8859-5",
    "iso-8859-6",
    "iso-8859-7",
    "iso-8859-8",
    "iso-8859-8-i",
    "iso-8859-10",
    "iso-8859-13",
    "iso-8859-14",
    "iso-8859-15",
    "iso-8859-16",
    "koi8-r",
    "koi8-u",
    "macintosh",
    "windows-874",
    "windows-1250",
    "windows-1251",
    "windows-1252",
    "windows-1253",
    "windows-1254",
    "windows-1255",
    "windows-1256",
    "windows-1257",
    "windows-1258",
    "x-mac-cyrillic",
    "gbk",
    "gb18030",
    "big5",
    "euc-jp",
    "iso-2022-jp",
    "shift_jis",
    "euc-kr",
    "x-user-defined",
    "utf-16le",
    "utf-16be",
  ];

  encodings.forEach(function(encoding) {
    assert_equals(new TextDecoder(encoding).encoding, encoding);
    assert_equals(new TextEncoder(encoding).encoding, "utf-8");
  });
}, "Non-UTF-8 encodings supported only for decode, not encode");
