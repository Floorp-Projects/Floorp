/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/SVG2/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface SVGImageElement : SVGGraphicsElement {
  [Constant]
  readonly attribute SVGAnimatedLength x;
  [Constant]
  readonly attribute SVGAnimatedLength y;
  [Constant]
  readonly attribute SVGAnimatedLength width;
  [Constant]
  readonly attribute SVGAnimatedLength height;
  [Constant]
  readonly attribute SVGAnimatedPreserveAspectRatio preserveAspectRatio;

  // Mirrored chrome-only nsIImageLoadingContent methods.  Please make sure
  // to update this list if nsIImageLoadingContent changes.
  [ChromeOnly]
  const long UNKNOWN_REQUEST = -1;
  [ChromeOnly]
  const long CURRENT_REQUEST = 0;
  [ChromeOnly]
  const long PENDING_REQUEST = 1;

  [ChromeOnly]
  attribute boolean loadingEnabled;
  [ChromeOnly]
  readonly attribute short imageBlockingStatus;
  [ChromeOnly]
  void addObserver(imgINotificationObserver aObserver);
  [ChromeOnly]
  void removeObserver(imgINotificationObserver aObserver);
  [ChromeOnly,Throws]
  imgIRequest? getRequest(long aRequestType);
  [ChromeOnly,Throws]
  long getRequestType(imgIRequest aRequest);
  [ChromeOnly,Throws]
  readonly attribute URI? currentURI;
  [ChromeOnly,Throws]
  nsIStreamListener? loadImageWithChannel(MozChannel aChannel);
  [ChromeOnly,Throws]
  void forceReload();
  [ChromeOnly]
  void forceImageState(boolean aForce, unsigned long long aState);
};

SVGImageElement implements SVGURIReference;

