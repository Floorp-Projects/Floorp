/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * https://drafts.csswg.org/cssom-view/#mediaquerylistevent
 */

[Exposed=Window]
interface MediaQueryListEvent : Event {
  constructor(DOMString type,
              optional MediaQueryListEventInit eventInitDict = {});

  readonly attribute UTF8String media;
  readonly attribute boolean matches;
};

dictionary MediaQueryListEventInit : EventInit {
  UTF8String media = "";
  boolean matches = false;
};
