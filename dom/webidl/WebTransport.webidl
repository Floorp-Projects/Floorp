/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* https://w3c.github.io/webtransport */

/* https://w3c.github.io/webtransport/#web-transport-configuration */

dictionary WebTransportHash {
  DOMString algorithm;
  BufferSource value;
};

dictionary WebTransportOptions {
  boolean allowPooling = false;
  boolean requireUnreliable = false;
  sequence<WebTransportHash> serverCertificateHashes;
  WebTransportCongestionControl congestionControl = "default";
};

enum WebTransportCongestionControl {
  "default",
  "throughput",
  "low-latency",
};

/* https://w3c.github.io/webtransport/#web-transport-close-info */

dictionary WebTransportCloseInfo {
  unsigned long closeCode = 0;
  UTF8String reason = "";
};

/* https://w3c.github.io/webtransport/#uni-stream-options */
dictionary WebTransportSendStreamOptions {
  long long? sendOrder = null;
};

/* https://w3c.github.io/webtransport/#web-transport-stats */

dictionary WebTransportStats {
  DOMHighResTimeStamp timestamp;
  unsigned long long bytesSent;
  unsigned long long packetsSent;
  unsigned long long packetsLost;
  unsigned long numOutgoingStreamsCreated;
  unsigned long numIncomingStreamsCreated;
  unsigned long long bytesReceived;
  unsigned long long packetsReceived;
  DOMHighResTimeStamp smoothedRtt;
  DOMHighResTimeStamp rttVariation;
  DOMHighResTimeStamp minRtt;
  WebTransportDatagramStats datagrams;
};

/* https://w3c.github.io/webtransport/#web-transport-stats%E2%91%A0 */

dictionary WebTransportDatagramStats {
  DOMHighResTimeStamp timestamp;
  unsigned long long expiredOutgoing;
  unsigned long long droppedIncoming;
  unsigned long long lostOutgoing;
};

/* https://w3c.github.io/webtransport/#web-transport */

[Exposed=(Window,Worker), SecureContext, Pref="network.webtransport.enabled"]
interface WebTransport {
  [Throws]
  constructor(USVString url, optional WebTransportOptions options = {});

  [NewObject]
  Promise<WebTransportStats> getStats();
  readonly attribute Promise<undefined> ready;
  readonly attribute WebTransportReliabilityMode reliability;
  readonly attribute WebTransportCongestionControl congestionControl;
  readonly attribute Promise<WebTransportCloseInfo> closed;
  [Throws] undefined close(optional WebTransportCloseInfo closeInfo = {});

  [Throws] readonly attribute WebTransportDatagramDuplexStream datagrams;

  [NewObject]
  Promise<WebTransportBidirectionalStream> createBidirectionalStream(
     optional WebTransportSendStreamOptions options = {});
  /* a ReadableStream of WebTransportBidirectionalStream objects */
  readonly attribute ReadableStream incomingBidirectionalStreams;


  /* XXX spec says this should be WebTransportSendStream */
  [NewObject]
  Promise<WritableStream> createUnidirectionalStream(
    optional WebTransportSendStreamOptions options = {});
  /* a ReadableStream of WebTransportReceiveStream objects */
  readonly attribute ReadableStream incomingUnidirectionalStreams;
};

enum WebTransportReliabilityMode {
  "pending",
  "reliable-only",
  "supports-unreliable",
};
