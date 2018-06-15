"use strict";

function run_test() {
  test_base64URLEncode();
  test_base64URLDecode();
}

// Test vectors from RFC 4648, section 10.
let textTests = {
  "": "",
  "f": "Zg",
  "fo": "Zm8",
  "foo": "Zm9v",
  "foob": "Zm9vYg",
  "fooba": "Zm9vYmE",
  "foobar": "Zm9vYmFy",
}

// Examples from RFC 4648, section 9.
let binaryTests = [{
  decoded: new Uint8Array([0x14, 0xfb, 0x9c, 0x03, 0xd9, 0x7e]),
  encoded: "FPucA9l-",
}, {
  decoded: new Uint8Array([0x14, 0xfb, 0x9c, 0x03, 0xd9]),
  encoded: "FPucA9k",
}, {
  decoded: new Uint8Array([0x14, 0xfb, 0x9c, 0x03]),
  encoded: "FPucAw",
}];

function padEncodedValue(value) {
  switch (value.length % 4) {
    case 0:
      return value;
    case 2:
      return value + "==";
    case 3:
      return value + "=";
    default:
      throw new TypeError("Invalid encoded value");
  }
}

function testEncode(input, encoded) {
  equal(ChromeUtils.base64URLEncode(input, { pad: false }),
        encoded, encoded + " without padding");
  equal(ChromeUtils.base64URLEncode(input, { pad: true }),
        padEncodedValue(encoded), encoded + " with padding");
}

function test_base64URLEncode() {
  throws(_ => ChromeUtils.base64URLEncode(new Uint8Array(0)), /TypeError/,
         "Should require encoding options");
  throws(_ => ChromeUtils.base64URLEncode(new Uint8Array(0), {}), /TypeError/,
         "Encoding should require the padding option");

  for (let {decoded, encoded} of binaryTests) {
    testEncode(decoded, encoded);
  }

  let textEncoder = new TextEncoder("utf-8");
  for (let decoded of Object.keys(textTests)) {
    let input = textEncoder.encode(decoded);
    testEncode(input, textTests[decoded]);
  }
}

function testDecode(input, decoded) {
  let buffer = ChromeUtils.base64URLDecode(input, { padding: "reject" });
  deepEqual(new Uint8Array(buffer), decoded, input + " with padding rejected");

  let paddedValue = padEncodedValue(input);
  buffer = ChromeUtils.base64URLDecode(paddedValue, { padding: "ignore" });
  deepEqual(new Uint8Array(buffer), decoded, input + " with padding ignored");

  if (paddedValue.length > input.length) {
    throws(_ => ChromeUtils.base64URLDecode(paddedValue, { padding: "reject" }),
           /NS_ERROR_ILLEGAL_VALUE/,
           paddedValue + " with padding rejected should throw");

    throws(_ => ChromeUtils.base64URLDecode(input, { padding: "require" }),
           /NS_ERROR_ILLEGAL_VALUE/,
           input + " with padding required should throw");

    buffer = ChromeUtils.base64URLDecode(paddedValue, { padding: "require" });
    deepEqual(new Uint8Array(buffer), decoded, paddedValue + " with padding required");
  }
}

function test_base64URLDecode() {
  throws(_ => ChromeUtils.base64URLDecode(""), /TypeError/,
         "Should require decoding options");
  throws(_ => ChromeUtils.base64URLDecode("", {}), /TypeError/,
         "Decoding should require the padding option");
  throws(_ => ChromeUtils.base64URLDecode("", { padding: "chocolate" }),
         /TypeError/,
         "Decoding should throw for invalid padding policy");

  for (let {decoded, encoded} of binaryTests) {
    testDecode(encoded, decoded);
  }

  let textEncoder = new TextEncoder("utf-8");
  for (let decoded of Object.keys(textTests)) {
    let expectedBuffer = textEncoder.encode(decoded);
    testDecode(textTests[decoded], expectedBuffer);
  }
}
