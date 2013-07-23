/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary SocketsDict {
  sequence<DOMString> host;
  sequence<unsigned long> port;
  sequence<boolean> active;
  sequence<unsigned long> tcp;
  sequence<double> socksent;
  sequence<double> sockreceived;
  double sent = 0;
  double received = 0;
};

dictionary HttpConnInfoDict {
  sequence<unsigned long> rtt;
  sequence<unsigned long> ttl;
  sequence<DOMString> protocolVersion;
};

dictionary HalfOpenInfoDict {
  sequence<boolean> speculative;
};

dictionary HttpConnDict {
  sequence<DOMString> host;
  sequence<unsigned long> port;
  sequence<HttpConnInfoDict> active;
  sequence<HttpConnInfoDict> idle;
  sequence<HalfOpenInfoDict> halfOpens;
  sequence<boolean> spdy;
  sequence<boolean> ssl;
};

dictionary WebSocketDict {
  sequence<DOMString> hostport;
  sequence<unsigned long> msgsent;
  sequence<unsigned long> msgreceived;
  sequence<double> sentsize;
  sequence<double> receivedsize;
  sequence<boolean> encrypted;
};

dictionary DNSCacheDict {
  sequence<DOMString> hostname;
  sequence<sequence<DOMString>> hostaddr;
  sequence<DOMString> family;
  sequence<double> expiration;
};
