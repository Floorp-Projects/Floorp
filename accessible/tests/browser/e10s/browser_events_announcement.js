/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `<p id="p">abc</p>`,
  async function (browser, accDoc) {
    let acc = findAccessibleChildByID(accDoc, "p");
    let onAnnounce = waitForEvent(EVENT_ANNOUNCEMENT, acc);
    acc.announce("please", nsIAccessibleAnnouncementEvent.POLITE);
    let evt = await onAnnounce;
    evt.QueryInterface(nsIAccessibleAnnouncementEvent);
    is(evt.announcement, "please", "announcement matches.");
    is(evt.priority, nsIAccessibleAnnouncementEvent.POLITE, "priority matches");

    onAnnounce = waitForEvent(EVENT_ANNOUNCEMENT, acc);
    acc.announce("do it", nsIAccessibleAnnouncementEvent.ASSERTIVE);
    evt = await onAnnounce;
    evt.QueryInterface(nsIAccessibleAnnouncementEvent);
    is(evt.announcement, "do it", "announcement matches.");
    is(
      evt.priority,
      nsIAccessibleAnnouncementEvent.ASSERTIVE,
      "priority matches"
    );
  },
  { iframe: true, remoteIframe: true }
);
