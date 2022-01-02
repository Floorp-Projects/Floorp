/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

enum RTCDataChannelState {
  "connecting",
  "open",
  "closing",
  "closed"
};

enum RTCDataChannelType {
  "arraybuffer",
  "blob"
};

[Exposed=Window]
interface RTCDataChannel : EventTarget
{
  readonly attribute DOMString label;
  readonly attribute boolean negotiated;
  readonly attribute boolean ordered;
  readonly attribute boolean reliable;
  readonly attribute unsigned short? maxPacketLifeTime;
  readonly attribute unsigned short? maxRetransmits;
  readonly attribute USVString protocol;
  readonly attribute unsigned short? id;
  readonly attribute RTCDataChannelState readyState;
  readonly attribute unsigned long bufferedAmount;
  attribute unsigned long bufferedAmountLowThreshold;
  attribute EventHandler onopen;
  attribute EventHandler onerror;
  attribute EventHandler onclose;
  void close();
  attribute EventHandler onmessage;
  attribute EventHandler onbufferedamountlow;
  attribute RTCDataChannelType binaryType;
  [Throws]
  void send(DOMString data);
  [Throws]
  void send(Blob data);
  [Throws]
  void send(ArrayBuffer data);
  [Throws]
  void send(ArrayBufferView data);
};
