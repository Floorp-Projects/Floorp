// NOTE: Requires testharness.js
// http://www.w3.org/2008/webapps/wiki/Harness

// Extension to testharness.js API which avoids logging enormous strings
// on a coding failure.
function assert_string_equals(actual, expected, description) {
  // short circuit success case
  if (actual === expected) {
    assert_true(true, description + ": <actual> === <expected>");
    return;
  }

  // length check
  assert_equals(
    actual.length,
    expected.length,
    description + ": string lengths"
  );

  var i, a, b;
  for (i = 0; i < actual.length; i++) {
    a = actual.charCodeAt(i);
    b = expected.charCodeAt(i);
    if (a !== b) {
      assert_true(
        false,
        description +
          ": code unit " +
          i.toString() +
          " unequal: " +
          cpname(a) +
          " != " +
          cpname(b)
      );
    } // doesn't return
  }

  // It should be impossible to get here, because the initial
  // comparison failed, so either the length comparison or the
  // codeunit-by-codeunit comparison should also fail.
  assert_true(false, description + ": failed to detect string difference");
}

// Inspired by:
// http://ecmanaut.blogspot.com/2006/07/encoding-decoding-utf8-in-javascript.html
function encode_utf8(string) {
  var utf8 = unescape(encodeURIComponent(string));
  var octets = new Uint8Array(utf8.length),
    i;
  for (i = 0; i < utf8.length; i += 1) {
    octets[i] = utf8.charCodeAt(i);
  }
  return octets;
}

function encode_utf16le(string) {
  var octets = new Uint8Array(string.length * 2);
  var di = 0;
  for (var i = 0; i < string.length; i++) {
    var code = string.charCodeAt(i);
    octets[di++] = code & 0xff;
    octets[di++] = code >> 8;
  }
  return octets;
}

function encode_utf16be(string) {
  var octets = new Uint8Array(string.length * 2);
  var di = 0;
  for (var i = 0; i < string.length; i++) {
    var code = string.charCodeAt(i);
    octets[di++] = code >> 8;
    octets[di++] = code & 0xff;
  }
  return octets;
}

function decode_utf8(octets) {
  var utf8 = String.fromCharCode.apply(null, octets);
  return decodeURIComponent(escape(utf8));
}

// Helpers for test_utf_roundtrip.
function cpname(n) {
  if (n + 0 !== n) {
    return n.toString();
  }
  var w = n <= 0xffff ? 4 : 6;
  return "U+" + ("000000" + n.toString(16).toUpperCase()).slice(-w);
}

function genblock(from, len) {
  var i, j, point, offset;
  var size, block;

  // determine size required:
  //    1 unit   for each point from U+000000 through U+00D7FF
  //    0 units                      U+00D800 through U+00DFFF
  //    1 unit                       U+00E000 through U+00FFFF
  //    2 units                      U+010000 through U+10FFFF
  function overlap(min1, max1, min2, max2) {
    return Math.max(0, Math.min(max1, max2) - Math.max(min1, min2));
  }
  size =
    overlap(from, from + len, 0x000000, 0x00d800) +
    overlap(from, from + len, 0x00e000, 0x010000) +
    overlap(from, from + len, 0x010000, 0x110000) * 2;

  block = new Uint16Array(size);
  for (i = 0, j = 0; i < len; i++) {
    point = from + i;
    if (0xd800 <= point && point <= 0xdfff) {
      continue;
    } else if (point <= 0xffff) {
      block[j++] = point;
    } else {
      offset = point - 0x10000;
      block[j++] = 0xd800 + (offset >> 10);
      block[j++] = 0xdc00 + (offset & 0x3ff);
    }
  }
  return String.fromCharCode.apply(null, block);
}

function test_utf_roundtrip() {
  var MIN_CODEPOINT = 0;
  var MAX_CODEPOINT = 0x10ffff;
  var BLOCK_SIZE = 0x1000;

  var block, block_tag, i, encoded, decoded, exp_encoded, exp_decoded;

  var TD_U16LE = new TextDecoder("UTF-16LE");

  var TD_U16BE = new TextDecoder("UTF-16BE");

  var TE_U8 = new TextEncoder();
  var TD_U8 = new TextDecoder("UTF-8");

  for (i = MIN_CODEPOINT; i < MAX_CODEPOINT; i += BLOCK_SIZE) {
    block_tag = cpname(i) + " - " + cpname(i + BLOCK_SIZE - 1);
    block = genblock(i, BLOCK_SIZE);

    // test UTF-16LE, UTF-16BE, and UTF-8 encodings against themselves
    encoded = encode_utf16le(block);
    decoded = TD_U16LE.decode(encoded);
    assert_string_equals(block, decoded, "UTF-16LE round trip " + block_tag);

    encoded = encode_utf16be(block);
    decoded = TD_U16BE.decode(encoded);
    assert_string_equals(block, decoded, "UTF-16BE round trip " + block_tag);

    encoded = TE_U8.encode(block);
    decoded = TD_U8.decode(encoded);
    assert_string_equals(block, decoded, "UTF-8 round trip " + block_tag);

    // test TextEncoder(UTF-8) against the older idiom
    exp_encoded = encode_utf8(block);
    assert_array_equals(
      encoded,
      exp_encoded,
      "UTF-8 reference encoding " + block_tag
    );

    exp_decoded = decode_utf8(exp_encoded);
    assert_string_equals(
      decoded,
      exp_decoded,
      "UTF-8 reference decoding " + block_tag
    );
  }
}

function test_utf_samples() {
  // z, cent, CJK water, G-Clef, Private-use character
  var sample = "z\xA2\u6C34\uD834\uDD1E\uDBFF\uDFFD";
  var cases = [
    {
      encoding: "utf-8",
      expected: [
        0x7a, 0xc2, 0xa2, 0xe6, 0xb0, 0xb4, 0xf0, 0x9d, 0x84, 0x9e, 0xf4, 0x8f,
        0xbf, 0xbd,
      ],
    },
    {
      encoding: "utf-16le",
      expected: [
        0x7a, 0x00, 0xa2, 0x00, 0x34, 0x6c, 0x34, 0xd8, 0x1e, 0xdd, 0xff, 0xdb,
        0xfd, 0xdf,
      ],
    },
    {
      encoding: "utf-16",
      expected: [
        0x7a, 0x00, 0xa2, 0x00, 0x34, 0x6c, 0x34, 0xd8, 0x1e, 0xdd, 0xff, 0xdb,
        0xfd, 0xdf,
      ],
    },
    {
      encoding: "utf-16be",
      expected: [
        0x00, 0x7a, 0x00, 0xa2, 0x6c, 0x34, 0xd8, 0x34, 0xdd, 0x1e, 0xdb, 0xff,
        0xdf, 0xfd,
      ],
    },
  ];

  var encoded = new TextEncoder().encode(sample);
  assert_array_equals(encoded, cases[0].expected, "expected equal encodings");

  cases.forEach(function (t) {
    var decoded = new TextDecoder(t.encoding).decode(
      new Uint8Array(t.expected)
    );
    assert_equals(decoded, sample, "expected equal decodings - " + t.encoding);
  });
}

test(
  test_utf_samples,
  "UTF-8, UTF-16LE, UTF-16BE - Encode/Decode - reference sample"
);

test(
  test_utf_roundtrip,
  "UTF-8, UTF-16LE, UTF-16BE - Encode/Decode - full roundtrip and " +
    "agreement with encode/decodeURIComponent"
);
