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
 * @typedef {import("./@types/perf").FileHandle} FileHandle
 */

// This worker uses the wasm module that was generated from https://github.com/mstange/profiler-get-symbols.
// See ProfilerGetSymbols.jsm for more information.
//
// The worker instantiates the module, reads the binary into wasm memory, runs
// the wasm code, and returns the symbol table or an error. Then it shuts down
// itself.

/* eslint camelcase: 0*/
const { getCompactSymbolTable, queryAPI } = wasm_bindgen;

// Read parts of an open OS.File instance into the Uint8Array dataBuf.
// This reads destBuf.byteLength bytes at offset offset.
function readFileBytesInto(file, offset, destBuf) {
  file.setPosition(offset);
  // We read the file in 4MB chunks. This limits the amount of temporary
  // memory we use while we copy the file contents into wasm memory.
  const chunkSize = 4 * 1024 * 1024;
  let posInDestBuf = 0;
  let remainingBytes = destBuf.byteLength;
  while (remainingBytes > 0) {
    const bytes = remainingBytes > chunkSize ? chunkSize : remainingBytes;
    const chunkData = file.read({ bytes });
    const chunkBytes = chunkData.byteLength;
    if (chunkBytes === 0) {
      break;
    }

    destBuf.set(chunkData, posInDestBuf);
    posInDestBuf += chunkBytes;
    remainingBytes -= chunkBytes;
  }
}

// Returns a plain object that is Structured Cloneable and has name and
// message properties.
function createPlainErrorObject(e) {
  if (e instanceof OS.File.Error) {
    // OS.File.Error has an empty message property; it constructs the error
    // message on-demand in its toString() method. So we handle those errors
    // specially.
    return {
      name: "OSFileError",
      message: e.toString(),
      fileName: e.fileName,
      lineNumber: e.lineNumber,
    };
  }
  // Regular errors: just rewrap the object.
  const { name, message, fileName, lineNumber } = e;
  return { name, message, fileName, lineNumber };
}

/**
 * A FileAndPathHelper object is passed to getCompactSymbolTable, which calls
 * the methods `getCandidatePathsForBinaryOrPdb` and `readFile` on it.
 */
class FileAndPathHelper {
  constructor(libInfoMap, objdirs) {
    this._libInfoMap = libInfoMap;
    this._objdirs = objdirs;
  }

  /**
   * Enumerate all paths at which we could find files with symbol information.
   * This method is called by wasm code (via the bindings).
   *
   * @param {string} debugName
   * @param {string} breakpadId
   * @returns {Array<string>}
   */
  getCandidatePathsForBinaryOrPdb(debugName, breakpadId) {
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
      candidatePaths.push(OS.Path.join(objdirPath, "dist", "bin", name));
      // Also search in the "objdir" directory itself (not just in dist/bin).
      // If, for some unforeseen reason, the relevant binary is not inside the
      // objdirs dist/bin/ directory, this provides a way out because it lets the
      // user specify the actual location.
      candidatePaths.push(OS.Path.join(objdirPath, name));
    }

    // Check the absolute paths of the library last.
    // We do this after the objdir search because the library's path may point
    // to a stripped binary, which will have fewer symbols than the original
    // binaries in the objdir.
    if (debugPath !== path) {
      // We're on Windows, and debugPath points to a PDB file.
      // On non-Windows, path and debugPath are always the same.

      // Check the PDB file before the binary because the PDB has the symbol
      // information. The binary is only used as a last-ditch fallback
      // for things like Windows system libraries (e.g. graphics drivers).
      candidatePaths.push(debugPath);
    }

    // The location of the binary. If the profile was obtained on this machine
    // (and not, for example, on an Android device), this file should always
    // exist.
    candidatePaths.push(path);

    return candidatePaths;
  }

  /**
   * Asynchronously prepare the file at `path` for synchronous reading.
   * This method is called by wasm code (via the bindings).
   *
   * @param {string} path
   * @returns {FileHandle}
   */
  async readFile(path) {
    const file = OS.File.open(path, { read: true });
    const info = file.stat();
    if (info.isDir) {
      throw new Error(`Path "${path}" is a directory.`);
    }

    const fileSize = info.size;

    // Create and return a FileHandle object. The methods of this object are
    // called by wasm code (via the bindings).
    return {
      getLength: () => fileSize,
      readBytesInto: (dest, offset) => {
        readFileBytesInto(file, offset, dest);
      },
      drop: () => {
        file.close();
      },
    };
  }
}

/** @param {MessageEvent<SymbolicationWorkerInitialMessage>} e */
onmessage = async e => {
  try {
    const { request, libInfoMap, objdirs, module } = e.data;

    if (!(module instanceof WebAssembly.Module)) {
      throw new Error("invalid WebAssembly module");
    }

    // Instantiate the WASM module.
    await wasm_bindgen(module);

    const helper = new FileAndPathHelper(libInfoMap, objdirs);

    switch (request.type) {
      case "GET_SYMBOL_TABLE": {
        const { debugName, breakpadId } = request;
        const result = await getCompactSymbolTable(
          debugName,
          breakpadId,
          helper
        );
        postMessage(
          { result },
          result.map(r => r.buffer)
        );
        break;
      }
      case "QUERY_SYMBOLICATION_API": {
        const { path, requestJson } = request;
        const result = await queryAPI(path, requestJson, helper);
        postMessage({ result });
        break;
      }
      default:
        throw new Error(`Unexpected request type ${request.type}`);
    }
  } catch (error) {
    postMessage({ error: createPlainErrorObject(error) });
  }
  close();
};
