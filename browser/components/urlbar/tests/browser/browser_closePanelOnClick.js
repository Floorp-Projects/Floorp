/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests that the urlbar panel closes when clicking certain ui elements.
 */

"use strict";

function synthesizeNativeMouseClick(elt) {
  let rect = elt.getBoundingClientRect();
  let win = elt.ownerGlobal;
  let x =
    (win.mozInnerScreenX + (rect.left + rect.right) / 2) *
    win.windowUtils.screenPixelsPerCSSPixel;
  let y =
    (win.mozInnerScreenY + (rect.top + rect.bottom) / 2) *
    win.windowUtils.screenPixelsPerCSSPixel;
  let nativeMessageMouseDown = 4;
  let nativeMessageMouseUp = 7;
  if (AppConstants.platform == "win") {
    nativeMessageMouseDown = 2;
    nativeMessageMouseUp = 4;
  } else if (AppConstants.platform == "macosx") {
    nativeMessageMouseDown = 1;
    nativeMessageMouseUp = 2;
  }

  return new Promise(resolve => {
    // mouseup is not handled yet on Windows, so just wait for mousedown.
    elt.addEventListener("mousedown", resolve, { capture: true, once: true });
    win.windowUtils.sendNativeMouseEvent(x, y, nativeMessageMouseDown, 0, null);
    win.windowUtils.sendNativeMouseEvent(x, y, nativeMessageMouseUp, 0, null);
  });
}

add_task(async function() {
  await BrowserTestUtils.withNewTab("about:robots", async () => {
    for (let elt of [
      gBrowser.selectedBrowser,
      gBrowser.tabContainer,
      document.querySelector("#nav-bar toolbarspring"),
    ]) {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        waitForFocus,
        value: "dummy",
      });
      // Must have at least one test.
      Assert.ok(!!elt, "Found a valid element: " + (elt.id || elt.localName));
      await UrlbarTestUtils.promisePopupClose(window, () =>
        synthesizeNativeMouseClick(elt)
      );
    }
  });
});
