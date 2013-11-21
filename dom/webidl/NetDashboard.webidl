/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary SocketElement {
  DOMString host = "";
  unsigned long port = 0;
  boolean active = false;
  boolean tcp = false;
  double sent = 0;
  double received = 0;
};

dictionary SocketsDict {
  sequence<SocketElement> sockets;
  double sent = 0;
  double received = 0;
};

dictionary HttpConnInfo {
  unsigned long rtt = 0;
  unsigned long ttl = 0;
  DOMString protocolVersion = "";
};

dictionary HalfOpenInfoDict {
  boolean speculative = false;
};

dictionary HttpConnectionElement {
  DOMString host = "";
  unsigned long port = 0;
  boolean spdy = false;
  boolean ssl = false;
  sequence<HttpConnInfo> active;
  sequence<HttpConnInfo> idle;
  sequence<HalfOpenInfoDict> halfOpens;
};

dictionary HttpConnDict {
  sequence<HttpConnectionElement> connections;
};

dictionary WebSocketElement {
  DOMString hostport = "";
  unsigned long msgsent = 0;
  unsigned long msgreceived = 0;
  double sentsize = 0;
  double receivedsize = 0;
  boolean encrypted = false;
};

dictionary WebSocketDict {
  sequence<WebSocketElement> websockets;
};

dictionary DnsCacheEntry {
  DOMString hostname = "";
  sequence<DOMString> hostaddr;
  DOMString family = "";
  double expiration = 0;
};

dictionary DNSCacheDict {
  sequence<DnsCacheEntry> entries;
};

dictionary DNSLookupDict {
  sequence<DOMString> address;
  DOMString error = "";
  boolean answer = false;
};

dictionary ConnStatusDict {
  DOMString status = "";
};
