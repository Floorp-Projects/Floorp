/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
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

  /**
   * Loads a XUL overlay and merges it with the current document, notifying an
   * observer when the merge is complete.
   * @param   url
   *          The URL of the overlay to load and merge
   * @param   observer
   *          An object implementing nsIObserver that will be notified with a
   *          message of topic "xul-overlay-merged" when the merge is complete.
   *          The subject parameter of |observe| will QI to a nsIURI - the URI
   *          of the merged overlay. This parameter is optional and may be null.
   *
   * NOTICE:  In the 2.0 timeframe this API will change such that the
   *          implementation will fire a DOMXULOverlayMerged event upon merge
   *          completion rather than notifying an observer. Do not rely on this
   *          API's behavior _not_ to change because it will!
   *          - Ben Goodger (8/23/2005)
   */
  [Throws]
  void loadOverlay(DOMString url, MozObserver? observer);
};
