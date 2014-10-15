/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is:
 * dom/interfaces/xul/nsIDOMXULDocument.idl
 */

interface XULCommandDispatcher;
interface MozObserver;

[Func="IsChromeOrXBL"]
interface XULDocument : Document {
           attribute Node? popupNode;

  /**
   * These attributes correspond to trustedGetPopupNode().rangeOffset and
   * rangeParent. They will help you find where in the DOM the popup is
   * happening. Can be accessed only during a popup event. Accessing any other
   * time will be an error.
   */
  [Throws, ChromeOnly]
  readonly attribute Node? popupRangeParent;
  [Throws, ChromeOnly]
  readonly attribute long  popupRangeOffset;

           attribute Node? tooltipNode;

  readonly attribute XULCommandDispatcher? commandDispatcher;

  [Throws]
  readonly attribute long width;
  [Throws]
  readonly attribute long height;

  NodeList getElementsByAttribute(DOMString name,
                                  [TreatNullAs=EmptyString] DOMString value);
  [Throws]
  NodeList getElementsByAttributeNS(DOMString? namespaceURI, DOMString name,
                                    [TreatNullAs=EmptyString] DOMString value);

  [Throws]
  void addBroadcastListenerFor(Element broadcaster, Element observer,
                               DOMString attr);
  void removeBroadcastListenerFor(Element broadcaster, Element observer,
                                  DOMString attr);

  [Throws]
  void persist([TreatNullAs=EmptyString] DOMString id, DOMString attr);

  [Throws]
  BoxObject? getBoxObjectFor(Element? element);

  [Throws]
  void loadOverlay(DOMString url, MozObserver? observer);
};
