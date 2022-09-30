/*
 * A helper class for working with SignalR handshakes.
 *
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
const TextMessageFormat = require("resource://devtools/client/netmonitor/src/components/messages/parsers/signalr/TextMessageFormat.js");
const Utils = require("resource://devtools/client/netmonitor/src/components/messages/parsers/signalr/Utils.js");
/** @private */
class HandshakeProtocol {
  // Handshake request is always JSON
  writeHandshakeRequest(handshakeRequest) {
    return TextMessageFormat.TextMessageFormat.write(
      JSON.stringify(handshakeRequest)
    );
  }
  parseHandshakeResponse(data) {
    let messageData;
    let remainingData;
    if (
      Utils.isArrayBuffer(data) ||
      // eslint-disable-next-line no-undef
      (typeof Buffer !== "undefined" && data instanceof Buffer)
    ) {
      // Format is binary but still need to read JSON text from handshake response
      const binaryData = new Uint8Array(data);
      const separatorIndex = binaryData.indexOf(
        TextMessageFormat.TextMessageFormat.RecordSeparatorCode
      );
      if (separatorIndex === -1) {
        throw new Error("Message is incomplete.");
      }
      // content before separator is handshake response
      // optional content after is additional messages
      const responseLength = separatorIndex + 1;
      messageData = String.fromCharCode.apply(
        null,
        binaryData.slice(0, responseLength)
      );
      remainingData =
        binaryData.byteLength > responseLength
          ? binaryData.slice(responseLength).buffer
          : null;
    } else {
      const textData = data;
      const separatorIndex = textData.indexOf(
        TextMessageFormat.TextMessageFormat.RecordSeparator
      );
      if (separatorIndex === -1) {
        throw new Error("Message is incomplete.");
      }
      // content before separator is handshake response
      // optional content after is additional messages
      const responseLength = separatorIndex + 1;
      messageData = textData.substring(0, responseLength);
      remainingData =
        textData.length > responseLength
          ? textData.substring(responseLength)
          : null;
    }
    // At this point we should have just the single handshake message
    const messages = TextMessageFormat.TextMessageFormat.parse(messageData);
    const response = JSON.parse(messages[0]);
    if (response.type) {
      throw new Error("Expected a handshake response from the server.");
    }
    const responseMessage = response;
    // multiple messages could have arrived with handshake
    // return additional data to be parsed as usual, or null if all parsed
    return [remainingData, responseMessage];
  }
}
exports.HandshakeProtocol = HandshakeProtocol;
