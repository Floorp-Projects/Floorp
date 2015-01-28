/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * For more information on this interface please see
 * http://dev.w3.org/2006/webapi/DOM-Level-3-Events/html/DOM3-Events.html
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface MouseEvent : UIEvent {
  readonly attribute long           screenX;
  readonly attribute long           screenY;
  readonly attribute long           clientX;
  readonly attribute long           clientY;
  readonly attribute boolean        ctrlKey;
  readonly attribute boolean        shiftKey;
  readonly attribute boolean        altKey;
  readonly attribute boolean        metaKey;
  readonly attribute short          button;
  readonly attribute unsigned short buttons;
  readonly attribute EventTarget?   relatedTarget;
  readonly attribute DOMString?     region;
  // Deprecated in DOM Level 3:
  [Throws]
  void                              initMouseEvent(DOMString typeArg, 
                                                   boolean canBubbleArg, 
                                                   boolean cancelableArg, 
                                                   Window? viewArg,
                                                   long detailArg, 
                                                   long screenXArg, 
                                                   long screenYArg, 
                                                   long clientXArg, 
                                                   long clientYArg, 
                                                   boolean ctrlKeyArg, 
                                                   boolean altKeyArg, 
                                                   boolean shiftKeyArg, 
                                                   boolean metaKeyArg, 
                                                   short buttonArg,
                                                   EventTarget? relatedTargetArg);
  // Introduced in DOM Level 3:
  boolean                           getModifierState(DOMString keyArg);
};


// Event Constructor Syntax:
[Constructor(DOMString typeArg, optional MouseEventInit mouseEventInitDict)]
partial interface MouseEvent
{
};

// Suggested initMouseEvent replacement initializer:
dictionary MouseEventInit : UIEventInit {
  // Attributes for MouseEvent:
  long           screenX       = 0;
  long           screenY       = 0;
  long           clientX       = 0;
  long           clientY       = 0;
  boolean        ctrlKey       = false;
  boolean        shiftKey      = false;
  boolean        altKey        = false;
  boolean        metaKey       = false;
  short          button        = 0;
  // Note: "buttons" was not previously initializable through initMouseEvent!
  unsigned short buttons       = 0;
  EventTarget?   relatedTarget = null;
};

// Mozilla extensions
partial interface MouseEvent
{
  readonly attribute long mozMovementX;
  readonly attribute long mozMovementY;

  // Finger or touch pressure event value
  // ranges between 0.0 and 1.0
  readonly attribute float mozPressure;

  const unsigned short    MOZ_SOURCE_UNKNOWN    = 0;
  const unsigned short    MOZ_SOURCE_MOUSE      = 1;
  const unsigned short    MOZ_SOURCE_PEN        = 2;
  const unsigned short    MOZ_SOURCE_ERASER     = 3;
  const unsigned short    MOZ_SOURCE_CURSOR     = 4;
  const unsigned short    MOZ_SOURCE_TOUCH      = 5;
  const unsigned short    MOZ_SOURCE_KEYBOARD   = 6;

  readonly attribute unsigned short mozInputSource;

  [Throws]
  void                initNSMouseEvent(DOMString typeArg,
                                       boolean canBubbleArg,
                                       boolean cancelableArg,
                                       Window? viewArg,
                                       long detailArg,
                                       long screenXArg,
                                       long screenYArg,
                                       long clientXArg,
                                       long clientYArg,
                                       boolean ctrlKeyArg,
                                       boolean altKeyArg,
                                       boolean shiftKeyArg,
                                       boolean metaKeyArg,
                                       short buttonArg,
                                       EventTarget? relatedTargetArg,
                                       float pressure,
                                       unsigned short inputSourceArg);
  [ChromeOnly]
  readonly attribute boolean hitCluster; // True when touch occurs in a cluster of links

};

