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
// Not exported from index
/** @private */
class TextMessageFormat {
  static write(output) {
    return `${output}${TextMessageFormat.RecordSeparator}`;
  }
  static parse(input) {
    if (input[input.length - 1] !== TextMessageFormat.RecordSeparator) {
      throw new Error("Message is incomplete.");
    }
    const messages = input.split(TextMessageFormat.RecordSeparator);
    messages.pop();
    return messages;
  }
}
exports.TextMessageFormat = TextMessageFormat;
TextMessageFormat.RecordSeparatorCode = 0x1e;
TextMessageFormat.RecordSeparator = String.fromCharCode(
  TextMessageFormat.RecordSeparatorCode
);
