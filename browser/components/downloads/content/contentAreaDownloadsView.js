/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from allDownloadsView.js */

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

var ContentAreaDownloadsView = {
  init() {
    let box = document.getElementById("downloadsRichListBox");
    box.addEventListener(
      "InitialDownloadsLoaded",
      () => {
        // Set focus to Downloads list once it is created
        // And prevent it from showing the focus ring around the richlistbox (Bug 1702694)
        document
          .getElementById("downloadsRichListBox")
          .focus({ preventFocusRing: true });
      },
      { once: true }
    );
    let view = new DownloadsPlacesView(box);
    // Do not display the Places downloads in private windows
    if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
      view.place = "place:transition=7&sort=4";
    }
  },
};

window.onload = function() {
  ContentAreaDownloadsView.init();
};
