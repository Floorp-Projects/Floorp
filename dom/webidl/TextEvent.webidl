/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/uievents/#textevent
 */

[Pref="dom.events.textevent.enabled", Exposed=Window]
interface TextEvent : UIEvent
{
  [NeedsSubjectPrincipal]
  readonly attribute DOMString data;

  undefined initTextEvent(DOMString type,
                     			optional boolean bubbles = false,
                    			optional boolean cancelable = false,
			                    optional Window? view = null,
			                    optional DOMString data = "undefined");
};
