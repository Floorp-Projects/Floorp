// |jit-test| --enable-arraybuffer-transfer; skip-if: !ArrayBuffer.prototype.transfer

const byteLengths = [
  // Byte lengths applicable for inline storage
  0, 1, 4, 16, 32, 64, 96,

  // Too large for inline storage
  128, 1024, 4096, 65536,
];

// Most values in |byteLengths| are powers-of-two. Add +1/-1 to test for
// off-by-one errors.
function offByOne(v) {
  if (v === 0) {
    return [v, v + 1];
  }
  return [v - 1, v, v + 1];
}

// Fast alternative to |assertEq| to avoid a slow VM-call.
// Should only be used when looping over the TypedArray contents to avoid
// unnecessary test slowdowns.
function assertEqNum(e, a) {
  if (e !== a) {
    assertEq(e, a);
  }
}

// Inline or malloced ArrayBuffer.
{
  for (let byteLength of byteLengths.flatMap(offByOne)) {
    for (let newByteLength of offByOne(byteLength)) {
      for (let transfer of [
        ArrayBuffer.prototype.transfer,
        ArrayBuffer.prototype.transferToFixedLength,
      ]) {
        let buffer = new ArrayBuffer(byteLength);
        let i8 = new Uint8Array(buffer);
        for (let i = 0; i < byteLength; ++i) {
          assertEqNum(i8[i], 0);
          i8[i] = i;
        }

        assertEq(buffer.byteLength, byteLength);
        assertEq(buffer.detached, false);

        let copy = transfer.call(buffer, newByteLength);

        assertEq(buffer.byteLength, 0);
        assertEq(buffer.detached, true);

        assertEq(copy.byteLength, newByteLength);
        assertEq(copy.detached, false);

        i8 = new Uint8Array(copy);
        for (let i = 0; i < Math.min(byteLength, newByteLength); ++i) {
          assertEqNum(i8[i], i & 0xff);
        }
        for (let i = byteLength; i < newByteLength; ++i) {
          assertEqNum(i8[i], 0);
        }
      }
    }
  }
}

// External buffer.
{
  for (let byteLength of byteLengths.flatMap(offByOne)) {
    for (let newByteLength of offByOne(byteLength)) {
      for (let transfer of [
        ArrayBuffer.prototype.transfer,
        ArrayBuffer.prototype.transferToFixedLength,
      ]) {
        let buffer = createExternalArrayBuffer(byteLength);
        let i8 = new Uint8Array(buffer);
        for (let i = 0; i < byteLength; ++i) {
          assertEqNum(i8[i], 0);
          i8[i] = i;
        }

        assertEq(buffer.byteLength, byteLength);
        assertEq(buffer.detached, false);

        let copy = transfer.call(buffer, newByteLength);

        assertEq(buffer.byteLength, 0);
        assertEq(buffer.detached, true);

        assertEq(copy.byteLength, newByteLength);
        assertEq(copy.detached, false);

        i8 = new Uint8Array(copy);
        for (let i = 0; i < Math.min(byteLength, newByteLength); ++i) {
          assertEqNum(i8[i], i & 0xff);
        }
        for (let i = byteLength; i < newByteLength; ++i) {
          assertEqNum(i8[i], 0);
        }
      }
    }
  }
}

// User-controlled buffer.
{
  for (let byteLength of byteLengths.flatMap(offByOne)) {
    for (let newByteLength of offByOne(byteLength)) {
      for (let transfer of [
        ArrayBuffer.prototype.transfer,
        ArrayBuffer.prototype.transferToFixedLength,
      ]) {
        let buffer = createUserArrayBuffer(byteLength);
        let i8 = new Uint8Array(buffer);
        for (let i = 0; i < byteLength; ++i) {
          assertEqNum(i8[i], 0);
          i8[i] = i;
        }

        assertEq(buffer.byteLength, byteLength);
        assertEq(buffer.detached, false);

        let copy = transfer.call(buffer, newByteLength);

        assertEq(buffer.byteLength, 0);
        assertEq(buffer.detached, true);

        assertEq(copy.byteLength, newByteLength);
        assertEq(copy.detached, false);

        i8 = new Uint8Array(copy);
        for (let i = 0; i < Math.min(byteLength, newByteLength); ++i) {
          assertEqNum(i8[i], i & 0xff);
        }
        for (let i = byteLength; i < newByteLength; ++i) {
          assertEqNum(i8[i], 0);
        }
      }
    }
  }
}

// Mapped file buffer.
if (getBuildConfiguration("mapped-array-buffer")) {
  let fileName = "arraybuffer-transfer-mapped.txt";
  let fileContent = os.file.readRelativeToScript(fileName, "binary");

  let byteLength = fileContent.byteLength;
  assertEq(byteLength > 0, true);

  for (let newByteLength of offByOne(byteLength)) {
    for (let transfer of [
      ArrayBuffer.prototype.transfer,
      ArrayBuffer.prototype.transferToFixedLength,
    ]) {
      let buffer = createMappedArrayBuffer(fileName);

      assertEq(buffer.byteLength, byteLength);
      assertEq(buffer.detached, false);

      let copy = transfer.call(buffer, newByteLength);

      assertEq(buffer.byteLength, 0);
      assertEq(buffer.detached, true);

      assertEq(copy.byteLength, newByteLength);
      assertEq(copy.detached, false);

      i8 = new Uint8Array(copy);
      for (let i = 0; i < Math.min(byteLength, newByteLength); ++i) {
        assertEqNum(i8[i], fileContent[i]);
      }
      for (let i = byteLength; i < newByteLength; ++i) {
        assertEqNum(i8[i], 0);
      }
    }
  }
}

// Cross-compartment
{
  let g = newGlobal({newCompartment: true});

  let byteLengthGetter = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, "byteLength").get;
  let detachedGetter = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, "detached").get;

  for (let byteLength of byteLengths) {
    for (let transfer of [
      ArrayBuffer.prototype.transfer,
      ArrayBuffer.prototype.transferToFixedLength,
    ]) {
      let buffer = new g.ArrayBuffer(byteLength);

      assertEq(byteLengthGetter.call(buffer), byteLength);
      assertEq(detachedGetter.call(buffer), false);

      let copy = transfer.call(buffer);

      assertEq(detachedGetter.call(buffer), true);
      assertEq(byteLengthGetter.call(buffer), 0);

      assertEq(detachedGetter.call(copy), false);
      assertEq(byteLengthGetter.call(copy), byteLength);

      // NOTE: Incorrect realm due to CallNonGenericMethod.
      assertEq(copy instanceof g.ArrayBuffer, true);
    }
  }
}
