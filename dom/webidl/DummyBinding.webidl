/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Dummy bindings that we need to force generation of things that
// aren't actually referenced anywhere in IDL yet but are used in C++.

[Global]
interface DummyInterface : EventTarget {
  readonly attribute OnErrorEventHandlerNonNull onErrorEventHandler;
  FilePropertyBag fileBag();
  InspectorRGBTriple rgbTriple();
  Function getFunction();
  void funcSocketsDict(optional SocketsDict arg);
  void funcHttpConnDict(optional HttpConnDict arg);
  void funcWebSocketDict(optional WebSocketDict arg);
  void funcDNSCacheDict(optional DNSCacheDict arg);
  void funcDNSLookupDict(optional DNSLookupDict arg);
  void funcConnStatusDict(optional ConnStatusDict arg);
  void frameRequestCallback(FrameRequestCallback arg);
  void MmsParameters(optional MmsParameters arg);
  void MmsAttachment(optional MmsAttachment arg);
  void AsyncScrollEventDetail(optional AsyncScrollEventDetail arg);
  void OpenWindowEventDetail(optional OpenWindowEventDetail arg);
  void DOMWindowResizeEventDetail(optional DOMWindowResizeEventDetail arg);
  void WifiOptions(optional WifiCommandOptions arg1,
                   optional WifiResultOptions arg2);
  void AppNotificationServiceOptions(optional AppNotificationServiceOptions arg);
  void AppInfo(optional AppInfo arg1);
};

interface DummyInterfaceWorkers {
  BlobPropertyBag blobBag();
};
