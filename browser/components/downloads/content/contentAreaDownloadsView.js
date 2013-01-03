/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

let ContentAreaDownloadsView = {
  init: function CADV_init() {
    let view = new DownloadsPlacesView(document.getElementById("downloadsRichListBox"));
    // Do not display the Places downloads in private windows
    if (!PrivateBrowsingUtils.isWindowPrivate(window)) {
      view.place = "place:transition=7&sort=4";
    }
  }
};
