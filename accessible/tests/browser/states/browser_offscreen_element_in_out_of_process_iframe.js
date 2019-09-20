/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const parentURL =
  "data:text/html;charset=utf-8," +
  '<div id="scroller" style="width: 300px; height: 300px; overflow-y: scroll; overflow-x: hidden;">' +
  '  <div style="width: 100%; height: 1000px;"></div>' +
  '  <iframe frameborder="0"/>' +
  "</div>";
const iframeURL =
  "data:text/html;charset=utf-8," +
  "<style>" +
  " html,body {" +
  "   /* Convenient for calculation of element positions */" +
  "   margin: 0;" +
  "   padding: 0;" +
  " }" +
  "</style>" +
  '<div id="target" style="width: 100px; height: 100px;">target</div>';

add_task(async function() {
  const win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  try {
    const browser = win.gBrowser.selectedTab.linkedBrowser;

    BrowserTestUtils.loadURI(browser, parentURL);
    await BrowserTestUtils.browserLoaded(browser, false, parentURL);

    async function setup(url) {
      const iframe = content.document.querySelector("iframe");

      iframe.contentWindow.location = url;
      await new Promise(resolve => {
        iframe.addEventListener("load", resolve, { once: true });
      });

      return iframe.browsingContext;
    }

    async function scrollTo(x, y) {
      await SpecialPowers.spawn(browser, [x, y], async (scrollX, scrollY) => {
        const scroller = content.document.getElementById("scroller");
        scroller.scrollTo(scrollX, scrollY);
        await new Promise(resolve => {
          scroller.addEventListener("scroll", resolve, { once: true });
        });
      });
      await waitForIFrameUpdates();
    }

    // Setup an out-of-process iframe which is initially scrolled out.
    const iframe = await SpecialPowers.spawn(browser, [iframeURL], setup);

    await waitForIFrameUpdates();
    await spawnTestStates(
      iframe,
      "target",
      nsIAccessibleStates.STATE_OFFSCREEN
    );

    // Scroll the iframe into view and the target element is also visible but
    // the visible area height is 11px.
    await scrollTo(0, 711);
    await spawnTestStates(
      iframe,
      "target",
      nsIAccessibleStates.STATE_OFFSCREEN
    );

    // Scroll to a position where the visible height is 13px.
    await scrollTo(0, 713);
    await spawnTestStates(iframe, "target", 0);

    // Scroll the iframe out again.
    await scrollTo(0, 0);
    await spawnTestStates(
      iframe,
      "target",
      nsIAccessibleStates.STATE_OFFSCREEN
    );
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
