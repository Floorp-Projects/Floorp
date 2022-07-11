/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from allDownloadsView.js */

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

var ContentAreaDownloadsView = {
  init() {
    let box = document.getElementById("downloadsListBox");
    let suppressionFlag = DownloadsCommon.SUPPRESS_CONTENT_AREA_DOWNLOADS_OPEN;
    box.addEventListener(
      "InitialDownloadsLoaded",
      () => {
        // Set focus to Downloads list once it is created
        // And prevent it from showing the focus ring around the richlistbox (Bug 1702694)
        document
          .getElementById("downloadsListBox")
          .focus({ focusVisible: false });
        // Pause the indicator if the browser is active.
        if (document.visibilityState === "visible") {
          DownloadsCommon.getIndicatorData(
            window
          ).attentionSuppressed |= suppressionFlag;
        }
      },
      { once: true }
    );
    let view = new DownloadsPlacesView(box, true, suppressionFlag);
    document.addEventListener("visibilitychange", aEvent => {
      let indicator = DownloadsCommon.getIndicatorData(window);
      if (document.visibilityState === "visible") {
        indicator.attentionSuppressed |= suppressionFlag;
      } else {
        indicator.attentionSuppressed &= ~suppressionFlag;
      }
    });
    // Do not display the Places downloads in private windows
    if (!PrivateBrowsingUtils.isContentWindowPrivate(window)) {
      view.place = "place:transition=7&sort=4";
    }
  },
};

window.onload = function() {
  ContentAreaDownloadsView.init();
};
