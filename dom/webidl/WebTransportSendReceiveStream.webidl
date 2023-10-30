/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* https://w3c.github.io/webtransport/#send-stream */


[Exposed=(Window,Worker), SecureContext, Pref="network.webtransport.enabled"]
interface WebTransportSendStream : WritableStream {
  attribute long long? sendOrder;
  Promise<WebTransportSendStreamStats> getStats();
};

/* https://w3c.github.io/webtransport/#send-stream-stats */

dictionary WebTransportSendStreamStats {
  DOMHighResTimeStamp timestamp;
  unsigned long long bytesWritten;
  unsigned long long bytesSent;
  unsigned long long bytesAcknowledged;
};

/* https://w3c.github.io/webtransport/#receive-stream */

[Exposed=(Window,Worker), SecureContext, Pref="network.webtransport.enabled"]
interface WebTransportReceiveStream : ReadableStream {
  Promise<WebTransportReceiveStreamStats> getStats();
};

/* https://w3c.github.io/webtransport/#receive-stream-stats */

dictionary WebTransportReceiveStreamStats {
  DOMHighResTimeStamp timestamp;
  unsigned long long bytesReceived;
  unsigned long long bytesRead;
};

/* https://w3c.github.io/webtransport/#bidirectional-stream */

[Exposed=(Window,Worker), SecureContext, Pref="network.webtransport.enabled"]
interface WebTransportBidirectionalStream {
  readonly attribute WebTransportReceiveStream readable;
  readonly attribute WebTransportSendStream writable;
};
