/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function getBuffer(size) {
  let buffer = new ArrayBuffer(size);
  is(buffer.byteLength, size, "Correct byte length");
  return buffer;
}

// May be called for any size, but you should call getBuffer() if you know
// that size is big and that randomness is not necessary because it is
// noticeably faster.
function getRandomBuffer(size) {
  let buffer = getBuffer(size);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < size; i++) {
    view[i] = parseInt(Math.random() * 255);
  }
  return buffer;
}

function compareBuffers(buffer1, buffer2) {
  if (buffer1.byteLength != buffer2.byteLength) {
    return false;
  }

  let view1 = buffer1 instanceof Uint8Array ? buffer1 : new Uint8Array(buffer1);
  let view2 = buffer2 instanceof Uint8Array ? buffer2 : new Uint8Array(buffer2);
  for (let i = 0; i < buffer1.byteLength; i++) {
    if (view1[i] != view2[i]) {
      return false;
    }
  }
  return true;
}
