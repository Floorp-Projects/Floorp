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

class PathHelper {
  constructor(libInfoMap, objdirs) {
    this._libInfoMap = libInfoMap;
    this._objdirs = objdirs;
  }

  /**
   * Enumerate all paths at which we could find files with symbol information.
   *
   * @param {string} debugName
   * @param {string} breakpadId
   * @returns {Array<{ path: string, debugPath: string }>}
   */
  getCandidatePaths(debugName, breakpadId) {
    const key = `${debugName}:${breakpadId}`;
    const lib = this._libInfoMap.get(key);
    if (!lib) {
      throw new Error(
        `Could not find the library for "${debugName}", "${breakpadId}".`
      );
    }

    const { name, path, debugPath } = lib;
    const candidatePaths = [];

    // First, try to find a binary with a matching file name and breakpadId
    // in one of the manually specified objdirs.
    // This is needed if the debuggee is a build running on a remote machine that
    // was compiled by the developer on *this* machine (the "host machine"). In
    // that case, the objdir will contain the compiled binary with full symbol and
    // debug information, whereas the binary on the device may not exist in
    // uncompressed form or may have been stripped of debug information and some
    // symbol information.
    // An objdir, or "object directory", is a directory on the host machine that's
    // used to store build artifacts ("object files") from the compilation process.
    // This only works if the binary is one of the Gecko binaries and not
    // a system library.
    for (const objdirPath of this._objdirs) {
      // Binaries are usually expected to exist at objdir/dist/bin/filename.
      candidatePaths.push({
        path: OS.Path.join(objdirPath, "dist", "bin", name),
        debugPath: OS.Path.join(objdirPath, "dist", "bin", name),
      });
      // Also search in the "objdir" directory itself (not just in dist/bin).
      // If, for some unforeseen reason, the relevant binary is not inside the
      // objdirs dist/bin/ directory, this provides a way out because it lets the
      // user specify the actual location.
      candidatePaths.push({
        path: OS.Path.join(objdirPath, name),
        debugPath: OS.Path.join(objdirPath, name),
      });
    }

    // Check the absolute paths of the library file(s) last.
    // We do this after the objdir search because the library's path may point
    // to a stripped binary, which will have fewer symbols than the original
    // binaries in the objdir.
    candidatePaths.push({ path, debugPath });

    return candidatePaths;
  }
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

function getSymbolTable(debugName, breakpadId, libInfoMap, objdirs) {
  const helper = new PathHelper(libInfoMap, objdirs);
  const candidatePaths = helper.getCandidatePaths(debugName, breakpadId);

  const errors = [];
  for (const { path, debugPath } of candidatePaths) {
    try {
      return getCompactSymbolTableFromPath(path, debugPath, breakpadId);
    } catch (e) {
      // getCompactSymbolTableFromPath was unsuccessful. So either the
      // file wasn't parseable or its contents didn't match the specified
      // breakpadId, or some other error occurred.
      // Advance to the next candidate path.
      errors.push(e);
    }
  }

  throw new Error(
    `Could not obtain symbols for the library ${debugName} ${breakpadId} ` +
      `because there was no matching file at any of the candidate paths: ${JSON.stringify(
        candidatePaths
      )}. Errors: ${errors.map(e => e.message).join(", ")}`
  );
}

/** @param {MessageEvent<SymbolicationWorkerInitialMessage>} e */
onmessage = async e => {
  try {
    const { debugName, breakpadId, libInfoMap, objdirs, module } = e.data;

    if (!(module instanceof WebAssembly.Module)) {
      throw new Error("invalid WebAssembly module");
    }

    // Instantiate the WASM module.
    await wasm_bindgen(module);

    const result = getSymbolTable(debugName, breakpadId, libInfoMap, objdirs);
    postMessage(
      { result },
      result.map(r => r.buffer)
    );
  } catch (error) {
    postMessage({ error: createPlainErrorObject(error) });
  }
  close();
};
