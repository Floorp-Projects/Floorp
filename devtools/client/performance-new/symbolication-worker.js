/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */

// FIXME: This file is currently not covered by TypeScript, there is no "@ts-check" comment.
// We should fix this once we know how to deal with the module imports below.
// (Maybe once Firefox supports worker module? Bug 1247687)

"use strict";

importScripts(
  "resource://gre/modules/osfile.jsm",
  "resource://devtools/client/performance-new/profiler_get_symbols.js"
);

/**
 * @typedef {import("./@types/perf").SymbolicationWorkerInitialMessage} SymbolicationWorkerInitialMessage
 */

// This worker uses the wasm module that was generated from https://github.com/mstange/profiler-get-symbols.
// See ProfilerGetSymbols.jsm for more information.
//
// The worker instantiates the module, reads the binary into wasm memory, runs
// the wasm code, and returns the symbol table or an error. Then it shuts down
// itself.

/* eslint camelcase: 0*/
const { WasmMemBuffer, get_compact_symbol_table } = wasm_bindgen;

// Read an open OS.File instance into the Uint8Array dataBuf.
function readFileInto(file, dataBuf) {
  // Ideally we'd be able to call file.readTo(dataBuf) here, but readTo no
  // longer exists.
  // So instead, we copy the file over into wasm memory in 4MB chunks. This
  // will take 425 invocations for a a 1.7GB file (such as libxul.so for a
  // Firefox for Android build) and not take up too much memory per call.
  const dataBufLen = dataBuf.byteLength;
  const chunkSize = 4 * 1024 * 1024;
  let pos = 0;
  while (pos < dataBufLen) {
    const chunkData = file.read({ bytes: chunkSize });
    const chunkBytes = chunkData.byteLength;
    if (chunkBytes === 0) {
      break;
    }

    dataBuf.set(chunkData, pos);
    pos += chunkBytes;
  }
}

// Returns a plain object that is Structured Cloneable and has name and
// description properties.
function createPlainErrorObject(e) {
  // OS.File.Error has an empty message property; it constructs the error
  // message on-demand in its toString() method. So we handle those errors
  // specially.
  if (!(e instanceof OS.File.Error)) {
    // Regular errors: just rewrap the object.
    if (e instanceof Error) {
      const { name, message, fileName, lineNumber } = e;
      return { name, message, fileName, lineNumber };
    }
    // The WebAssembly code throws errors with fields error_type and error_msg.
    if (e.error_type) {
      return {
        name: e.error_type,
        message: e.error_msg,
      };
    }
  }

  return {
    name: e instanceof OS.File.Error ? "OSFileError" : "Error",
    message: e.toString(),
    fileName: e.fileName,
    lineNumber: e.lineNumber,
  };
}

function getCompactSymbolTableFromPath(binaryPath, debugPath, breakpadId) {
  // Read the binary file into WASM memory.
  const binaryFile = OS.File.open(binaryPath, { read: true });
  const binaryData = new WasmMemBuffer(binaryFile.stat().size, array => {
    readFileInto(binaryFile, array);
  });
  binaryFile.close();

  // Do the same for the debug file, if it is supplied and different from the
  // binary file. This is only the case on Windows.
  let debugData = binaryData;
  if (debugPath && debugPath !== binaryPath) {
    const debugFile = OS.File.open(debugPath, { read: true });
    debugData = new WasmMemBuffer(debugFile.stat().size, array => {
      readFileInto(debugFile, array);
    });
    debugFile.close();
  }

  try {
    const output = get_compact_symbol_table(binaryData, debugData, breakpadId);
    const result = [
      output.take_addr(),
      output.take_index(),
      output.take_buffer(),
    ];
    output.free();
    return result;
  } finally {
    binaryData.free();
    if (debugData != binaryData) {
      debugData.free();
    }
  }
}

/** @param {MessageEvent<SymbolicationWorkerInitialMessage>} e */
onmessage = async e => {
  try {
    const { binaryPath, debugPath, breakpadId, module } = e.data;

    if (!(module instanceof WebAssembly.Module)) {
      throw new Error("invalid WebAssembly module");
    }

    // Instantiate the WASM module.
    await wasm_bindgen(module);

    const result = getCompactSymbolTableFromPath(
      binaryPath,
      debugPath,
      breakpadId
    );
    postMessage(
      { result },
      result.map(r => r.buffer)
    );
  } catch (error) {
    postMessage({ error: createPlainErrorObject(error) });
  }
  close();
};
