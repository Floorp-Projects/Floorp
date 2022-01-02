load(libdir + 'asserts.js');

function typedArrayMatchesString(ta, str, uptoStringLength = false) {
  if (ta.length != str.length && !uptoStringLength)
    return false;
  for (let i = 0; i < str.length; i++) {
    if (ta[i] != str.charCodeAt(i))
      return false;
  }
  return true;
}

function test() {

  // Check that {signed,unsigned} -> {Int*Array, Uint*Array} for string types.

  const shortU8 = ctypes.unsigned_char.array(10)("abc\0\0\0\0\0\0\0").readTypedArray();
  assertEq(shortU8 instanceof Uint8Array, true);
  assertEq(typedArrayMatchesString(shortU8, "abc\0\0\0\0\0\0\0"), true);

  const shortI8 = ctypes.signed_char.array(10)("abc\0\0\0\0\0\0\0").readTypedArray();
  assertEq(shortI8 instanceof Int8Array, true);
  assertEq(typedArrayMatchesString(shortI8, "abc\0\0\0\0\0\0\0"), true);

  const shortU16 = ctypes.char16_t.array(10)("千").readTypedArray();
  assertEq(shortU16 instanceof Uint16Array, true);
  assertEq(typedArrayMatchesString(shortU16, "千", 'ignore zero-padding, please'), true);

  // ...and for (other) numeric types.

  const I16 = ctypes.int16_t.array(10)().readTypedArray();
  assertEq(I16 instanceof Int16Array, true);

  const U32 = ctypes.uint32_t.array(10)().readTypedArray();
  assertEq(U32 instanceof Uint32Array, true);

  const I32 = ctypes.int32_t.array(10)().readTypedArray();
  assertEq(I32 instanceof Int32Array, true);

  // Check that pointers without associated length get truncated to strlen().

  const unsignedCharArray = ctypes.unsigned_char.array(10)("abc\0\0\0");
  const shortU8cs = unsignedCharArray.addressOfElement(0).readTypedArray();
  assertEq(shortU8cs instanceof Uint8Array, true);
  assertEq(shortU8cs.length, 3);
  assertEq(typedArrayMatchesString(shortU8cs, "abc", 'stop at NUL, please'), true);

  const signedCharArray = ctypes.signed_char.array(10)("abc\0\0\0");
  const shortI8cs = signedCharArray.addressOfElement(0).readTypedArray();
  assertEq(shortI8cs instanceof Int8Array, true);
  assertEq(shortI8cs.length, 3);
  assertEq(typedArrayMatchesString(shortI8cs, "abc", 'stop at NUL, please'), true);

  const char16Array = ctypes.char16_t.array(10)("千\0");
  const shortU16cs = char16Array.addressOfElement(0).readTypedArray();
  assertEq(shortU16cs instanceof Uint16Array, true);
  assertEq(shortU16cs.length, 1);
  assertEq(typedArrayMatchesString(shortU16cs, "千", 'ignore zero-padding, please'), true);

  // Other types should just fail if the length is not known.

  assertTypeErrorMessage(() => { ctypes.int32_t.array(3)().addressOfElement(0).readTypedArray(); },
                         /base type .* is not an 8-bit or 16-bit integer or character type/);
  assertTypeErrorMessage(() => { ctypes.float.array(3)().addressOfElement(0).readTypedArray(); },
                         /base type .* is not an 8-bit or 16-bit integer or character type/);
  assertTypeErrorMessage(() => { ctypes.double.array(3)().addressOfElement(0).readTypedArray(); },
                         /base type .* is not an 8-bit or 16-bit integer or character type/);
  // char16_t is unsigned, so this is not a string type.
  assertTypeErrorMessage(() => { ctypes.int16_t.array(3)().addressOfElement(0).readTypedArray(); },
                         /base type .* is not an 8-bit or 16-bit integer or character type/);

  // Wide string -> bytes -> UTF-8 encoded string in typed array

  const input2 = "千千千千千千千千千千千千千千千千千千千千千千千千千";
  const encoded = ctypes.char.array(input2.length * 3)(input2).readTypedArray();
  // Each 千 character is encoded as 3 bytes.
  assertEq(encoded.length, input2.length * 3);

  // Wide string -> char16_t -> 16-bit encoded string in typed array

  const encoded16 = ctypes.char16_t.array(input2.length)(input2).readTypedArray();
  assertEq(encoded16.length, input2.length);

  // Floats

  const floats = ctypes.float.array(3)([10, 20, 30]).readTypedArray();
  assertEq(floats instanceof Float32Array, true);
  assertEq(floats.toString(), "10,20,30");

  const doubles = ctypes.double.array(3)([10, 20, 30]).readTypedArray();
  assertEq(doubles instanceof Float64Array, true);
  assertEq(doubles.toString(), "10,20,30");

  // Invalid

  assertTypeErrorMessage(() => { ctypes.int64_t.array(3)([10, 20, 30]).readTypedArray() },
                         /base type ctypes.int64_t.array.*is not compatible with a typed array element type/);

}

if (typeof ctypes === "object")
  test();
