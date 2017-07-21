/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported ParseSymbols */

var EXPORTED_SYMBOLS = ["ParseSymbols"];

function convertStringArrayToUint8BufferWithIndex(array, approximateLength) {
  const index = new Uint32Array(array.length + 1);

  const textEncoder = new TextEncoder();
  let buffer = new Uint8Array(approximateLength);
  let pos = 0;

  for (let i = 0; i < array.length; i++) {
    const encodedString = textEncoder.encode(array[i]);

    let size = pos + buffer.length;
    if (size < buffer.length) {
      size = 2 << Math.log(size) / Math.log(2);
      let newBuffer = new Uint8Array(size);
      newBuffer.set(buffer);
      buffer = newBuffer;
    }

    buffer.set(encodedString, pos);
    index[i] = pos;
    pos += encodedString.length;
  }
  index[array.length] = pos;

  return {index, buffer};
}

function convertSymsMapToExpectedSymFormat(syms, approximateSymLength) {
  const addresses = Array.from(syms.keys());
  addresses.sort((a, b) => a - b);

  const symsArray = addresses.map(addr => syms.get(addr));
  const {index, buffer} =
    convertStringArrayToUint8BufferWithIndex(symsArray, approximateSymLength);

  return [new Uint32Array(addresses), index, buffer];
}

function convertSymsMapToDemanglerFormat(syms) {
  const addresses = Array.from(syms.keys());
  addresses.sort((a, b) => a - b);

  const symsArray = addresses.map(addr => syms.get(addr));
  const textEncoder = new TextEncoder();
  const buffer = textEncoder.encode(symsArray.join("\n"));

  return [new Uint32Array(addresses), buffer];
}

var ParseSymbols = {
  convertSymsMapToExpectedSymFormat,
  convertSymsMapToDemanglerFormat,
};
