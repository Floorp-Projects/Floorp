/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const parentPATH = DIRPATH + "empty.html";
const iframePATH = DIRPATH + "empty.html";

const parentURL = `http://example.com/${parentPATH}`;
const iframeURL = `http://example.org/${iframePATH}`;

add_task(async function() {
  const win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  try {
    const browser = win.gBrowser.selectedTab.linkedBrowser;

    BrowserTestUtils.loadURI(browser, parentURL);
    await BrowserTestUtils.browserLoaded(browser, false, parentURL);

    async function setup(url) {
      const scroller = content.document.createElement("div");
      scroller.style = "width: 300px; height: 300px; overflow: scroll;";
      scroller.setAttribute("id", "scroller");
      content.document.body.appendChild(scroller);

      // Make a space bigger than display port.
      const spacer = content.document.createElement("div");
      spacer.style = "width: 100%; height: 2000vh;";
      scroller.appendChild(spacer);

      const iframe = content.document.createElement("iframe");
      scroller.appendChild(iframe);

      iframe.contentWindow.location = url;
      await new Promise(resolve => {
        iframe.addEventListener("load", resolve, { once: true });
      });

      return iframe.browsingContext;
    }

    // Setup an iframe which is initially out of the display port.
    const iframe = await SpecialPowers.spawn(browser, [iframeURL], setup);

    await new Promise(resolve => requestAnimationFrame(resolve));

    await SpecialPowers.spawn(iframe, [], () => {
      const docShell = SpecialPowers.wrap(content.window).docShell;
      Assert.ok(!docShell.isActive, "the doc shell should be inactive");
    });
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
