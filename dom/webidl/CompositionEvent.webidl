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

interface WindowProxy;

interface CompositionEvent : UIEvent
{
  readonly attribute DOMString? data;
  readonly attribute DOMString  locale;
};

partial interface CompositionEvent
{
  [Throws]
  void initCompositionEvent(DOMString typeArg,
                            boolean canBubbleArg,
                            boolean cancelableArg,
                            WindowProxy? viewArg,
                            DOMString? dataArg,
                            DOMString localeArg);
};
