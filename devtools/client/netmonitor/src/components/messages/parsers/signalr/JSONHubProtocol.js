/*
 * Implements the SignalR Hub Protocol.
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
const IHubProtocol = require("resource://devtools/client/netmonitor/src/components/messages/parsers/signalr/IHubProtocol.js");
const TextMessageFormat = require("resource://devtools/client/netmonitor/src/components/messages/parsers/signalr/TextMessageFormat.js");
/** Implements the JSON Hub Protocol. */
class JsonHubProtocol {
  /** Creates an array of {@link @microsoft/signalr.HubMessage} objects from the specified serialized representation.
   *
   * @param {string} input A string containing the serialized representation.
   */
  parseMessages(input) {
    // The interface does allow "ArrayBuffer" to be passed in, but this implementation does not. So let's throw a useful error.
    if (typeof input !== "string") {
      throw new Error(
        "Invalid input for JSON hub protocol. Expected a string."
      );
    }
    if (!input) {
      return [];
    }
    // Parse the messages
    const messages = TextMessageFormat.TextMessageFormat.parse(input);
    const hubMessages = [];
    for (const message of messages) {
      const parsedMessage = JSON.parse(message);
      if (typeof parsedMessage.type !== "number") {
        throw new Error("Invalid payload.");
      }
      switch (parsedMessage.type) {
        case IHubProtocol.MessageType.Invocation:
          this.isInvocationMessage(parsedMessage);
          break;
        case IHubProtocol.MessageType.StreamItem:
          this.isStreamItemMessage(parsedMessage);
          break;
        case IHubProtocol.MessageType.Completion:
          this.isCompletionMessage(parsedMessage);
          break;
        case IHubProtocol.MessageType.Ping:
          // Single value, no need to validate
          break;
        case IHubProtocol.MessageType.Close:
          // All optional values, no need to validate
          break;
        default:
          // Future protocol changes can add message types, new kinds of messages
          // will show up without having to update Firefox.
          break;
      }
      // Map numeric message type to their textual name if it exists
      parsedMessage.type =
        IHubProtocol.MessageType[parsedMessage.type] || parsedMessage.type;
      hubMessages.push(parsedMessage);
    }
    return hubMessages;
  }
  /** Writes the specified {@link @microsoft/signalr.HubMessage} to a string and returns it.
   *
   * @param {HubMessage} message The message to write.
   * @returns {string} A string containing the serialized representation of the message.
   */
  writeMessage(message) {
    return TextMessageFormat.TextMessageFormat.write(JSON.stringify(message));
  }
  isInvocationMessage(message) {
    this.assertNotEmptyString(
      message.target,
      "Invalid payload for Invocation message."
    );
    if (message.invocationId !== undefined) {
      this.assertNotEmptyString(
        message.invocationId,
        "Invalid payload for Invocation message."
      );
    }
  }
  isStreamItemMessage(message) {
    this.assertNotEmptyString(
      message.invocationId,
      "Invalid payload for StreamItem message."
    );
    if (message.item === undefined) {
      throw new Error("Invalid payload for StreamItem message.");
    }
  }
  isCompletionMessage(message) {
    if (message.result && message.error) {
      throw new Error("Invalid payload for Completion message.");
    }
    if (!message.result && message.error) {
      this.assertNotEmptyString(
        message.error,
        "Invalid payload for Completion message."
      );
    }
    this.assertNotEmptyString(
      message.invocationId,
      "Invalid payload for Completion message."
    );
  }
  assertNotEmptyString(value, errorMessage) {
    if (typeof value !== "string" || value === "") {
      throw new Error(errorMessage);
    }
  }
}
exports.JsonHubProtocol = JsonHubProtocol;
