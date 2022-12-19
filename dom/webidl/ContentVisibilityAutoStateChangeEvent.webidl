/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * https://drafts.csswg.org/css-contain-2/#content-visibility-auto-state-changed
 */

[Exposed=Window, Pref="layout.css.content-visibility.enabled"]
interface ContentVisibilityAutoStateChangeEvent : Event {
  constructor(DOMString type,
              optional ContentVisibilityAutoStateChangeEventInit eventInitDict = {});
  readonly attribute boolean skipped;
};

dictionary ContentVisibilityAutoStateChangeEventInit : EventInit {
  boolean skipped = false;
};
