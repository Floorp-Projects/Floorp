/*
 * Copyright (c) .NET Foundation. All rights reserved.
 *
 * This source code is licensed under the Apache License, Version 2.0,
 * found in the LICENSE.txt file in the root directory of the library
 * source tree.
 *
 * https://github.com/aspnet/AspNetCore
 */

"use strict";

Object.defineProperty(exports, "__esModule", { value: true });
// Also in signalr-protocol-msgpack/Utils.ts
/** @private */
function isArrayBuffer(val) {
  return (
    val &&
    typeof ArrayBuffer !== "undefined" &&
    (val instanceof ArrayBuffer ||
      // Sometimes we get an ArrayBuffer that doesn't satisfy instanceof
      (val.constructor && val.constructor.name === "ArrayBuffer"))
  );
}
exports.isArrayBuffer = isArrayBuffer;
