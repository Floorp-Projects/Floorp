/*
 * A protocol abstraction for communicating with SignalR hubs.
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
/** Defines the type of a Hub Message. */
var MessageType;
(function (_MessageType) {
  /** Indicates the message is an Invocation message and implements the {@link @microsoft/signalr.InvocationMessage} interface. */
  MessageType[(MessageType.Invocation = 1)] = "Invocation";
  /** Indicates the message is a StreamItem message and implements the {@link @microsoft/signalr.StreamItemMessage} interface. */
  MessageType[(MessageType.StreamItem = 2)] = "StreamItem";
  /** Indicates the message is a Completion message and implements the {@link @microsoft/signalr.CompletionMessage} interface. */
  MessageType[(MessageType.Completion = 3)] = "Completion";
  /** Indicates the message is a Stream Invocation message and implements the {@link @microsoft/signalr.StreamInvocationMessage} interface. */
  MessageType[(MessageType.StreamInvocation = 4)] = "StreamInvocation";
  /** Indicates the message is a Cancel Invocation message and implements the {@link @microsoft/signalr.CancelInvocationMessage} interface. */
  MessageType[(MessageType.CancelInvocation = 5)] = "CancelInvocation";
  /** Indicates the message is a Ping message and implements the {@link @microsoft/signalr.PingMessage} interface. */
  MessageType[(MessageType.Ping = 6)] = "Ping";
  /** Indicates the message is a Close message and implements the {@link @microsoft/signalr.CloseMessage} interface. */
  MessageType[(MessageType.Close = 7)] = "Close";
})((MessageType = exports.MessageType || (exports.MessageType = {})));
