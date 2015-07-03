/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#htmlimageelement
 * http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

interface imgINotificationObserver;
interface imgIRequest;
interface URI;
interface MozChannel;
interface nsIStreamListener;

[NamedConstructor=Image(optional unsigned long width, optional unsigned long height)]
interface HTMLImageElement : HTMLElement {
           [SetterThrows]
           attribute DOMString alt;
           [SetterThrows]
           attribute DOMString src;
           [SetterThrows, Pref="dom.image.srcset.enabled"]
           attribute DOMString srcset;
           [SetterThrows]
           attribute DOMString? crossOrigin;
           [SetterThrows]
           attribute DOMString useMap;
           [SetterThrows]
           attribute DOMString referrer;
           [SetterThrows]
           attribute boolean isMap;
           [SetterThrows]
           attribute unsigned long width;
           [SetterThrows]
           attribute unsigned long height;
  readonly attribute unsigned long naturalWidth;
  readonly attribute unsigned long naturalHeight;
  readonly attribute boolean complete;
};

// http://www.whatwg.org/specs/web-apps/current-work/#other-elements,-attributes-and-apis
partial interface HTMLImageElement {
           [SetterThrows]
           attribute DOMString name;
           [SetterThrows]
           attribute DOMString align;
           [SetterThrows]
           attribute unsigned long hspace;
           [SetterThrows]
           attribute unsigned long vspace;
           [SetterThrows]
           attribute DOMString longDesc;

  [TreatNullAs=EmptyString,SetterThrows] attribute DOMString border;
};

// [Update me: not in whatwg spec yet]
// http://picture.responsiveimages.org/#the-img-element
partial interface HTMLImageElement {
           [SetterThrows, Pref="dom.image.picture.enabled"]
           attribute DOMString sizes;
           [Pref="dom.image.srcset.enabled"]
  readonly attribute DOMString currentSrc;
};

// Mozilla extensions.
partial interface HTMLImageElement {
           attribute DOMString lowsrc;

  // These attributes are offsets from the closest view (to mimic
  // NS4's "offset-from-layer" behavior).
  readonly attribute long x;
  readonly attribute long y;
};

[NoInterfaceObject]
interface MozImageLoadingContent {
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
  void forceReload(optional boolean aNotify);
  [ChromeOnly]
  void forceImageState(boolean aForce, unsigned long long aState);
};

HTMLImageElement implements MozImageLoadingContent;
