/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * http://dev.w3.org/2006/webapi/DOM-Level-3-Events/html/DOM3-Events.html#events-CompositionEvent
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface CompositionEvent : UIEvent
{
  readonly attribute DOMString? data;
  readonly attribute DOMString  locale;

 /**
  * ranges is trying to expose TextRangeArray in Gecko so a
  * js-plugin couble be able to know the clauses information
  */
  [ChromeOnly,Cached,Pure]
  readonly attribute sequence<TextClause> ranges;
};

partial interface CompositionEvent
{
  void initCompositionEvent(DOMString typeArg,
                            boolean canBubbleArg,
                            boolean cancelableArg,
                            Window? viewArg,
                            DOMString? dataArg,
                            DOMString localeArg);
};
