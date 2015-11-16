/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

var ContentAreaDownloadsView = {
  init() {
    let view = new DownloadsPlacesView(document.getElementById("downloadsRichListBox"));
    // Do not display the Places downloads in private windows
    if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
      view.place = "place:transition=7&sort=4";
    }
    // Set focus to Downloads list once it is created
    document.getElementById("downloadsRichListBox").focus();
  },
};
