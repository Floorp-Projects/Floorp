/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Simple wasm parser to replace "sourceMappingURL" section.

function updateSourceMappingURLSection(buffer, sourceMapUrl) {
  function readVarUint8(buf, pos) {
    let b = buf[pos++];
    let shift = 0;
    let result = 0;
    while (b & 0x80) {
      result |= (b & 0x7f) << shift;
      shift += 7;
      b = buf[pos++];
    }
    return {
      value: result | (b << shift),
      pos
    };
  }
  function readWasmString(buf, pos) {
    const { pos: next, value: len } = readVarUint8(buf, pos);
    const result = String.fromCharCode.apply(
      null,
      buf.subarray(next, next + len)
    );
    return { value: result, pos: next + len };
  }
  function toVarUint(n) {
    const buf = [];
    while (n > 127) {
      buf.push((n & 0x7f) | 0x80);
      n >>>= 7;
    }
    buf.push(n);
    return buf;
  }
  function toWasmString(s) {
    const buf = toVarUint(s.length);
    for (let i = 0; i < s.length; i++) {
      buf.push(s.charCodeAt(i));
    }
    return buf;
  }

  // Appending/replacing sourceMappingURL section based on
  // https://github.com/WebAssembly/design/pull/1051
  const mappingSectionBody = toWasmString("sourceMappingURL").concat(
    toWasmString(sourceMapUrl)
  );
  const mappingSection = toVarUint(0).concat(
    toVarUint(mappingSectionBody.length),
    mappingSectionBody
  );
  const data = new Uint8Array(buffer);
  let start = data.length,
    end = data.length;
  for (let i = 8; i < data.length; ) {
    const { pos: next, value: id } = readVarUint8(data, i);
    const { pos: next2, value: size } = readVarUint8(data, next);
    if (id == 0 && readWasmString(data, next2).value === "sourceMappingURL") {
      start = i;
      end = next2 + size;
      break;
    }
    i = next2 + size;
  }
  const result = new Uint8Array(
    start + (data.length - end) + mappingSection.length
  );
  result.set(data.subarray(0, start));
  result.set(new Uint8Array(mappingSection), start);
  result.set(data.subarray(end), start + mappingSection.length);
  return result.buffer;
}
