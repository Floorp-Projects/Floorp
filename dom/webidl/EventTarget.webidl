/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-dom-20120105/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface EventTarget {
  /* Passing null for wantsUntrusted means "default behavior", which
     differs in content and chrome.  In content that default boolean
     value is true, while in chrome the default boolean value is
     false. */
  [Throws]
  void addEventListener(DOMString type,
                        EventListener? listener,
                        optional boolean capture = false,
                        optional boolean? wantsUntrusted = null);
  [Throws]
  void removeEventListener(DOMString type,
                           EventListener? listener,
                           optional boolean capture = false);
  [Throws]
  boolean dispatchEvent(Event event);
};

// Mozilla extensions for use by JS-implemented event targets to
// implement on* properties.
partial interface EventTarget {
  [ChromeOnly, Throws]
  void setEventHandler(DOMString type, EventHandler handler);

  [ChromeOnly]
  EventHandler getEventHandler(DOMString type);
};
