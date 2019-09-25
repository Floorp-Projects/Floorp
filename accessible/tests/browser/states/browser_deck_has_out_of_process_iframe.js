/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const parentPATH = DIRPATH + "test_deck_has_out_of_process_iframe.xul";
const iframePATH = DIRPATH + "target.html";

// XXX: Using external files here since using data URL breaks something, e.g. it
// makes querying the second iframe in a hidden deck failure for some reasons.
const parentURL = `http://example.com/${parentPATH}`;
const iframeURL = `http://example.org/${iframePATH}`;

add_task(async function() {
  if (Preferences.locked("fission.autostart")) {
    ok(
      true,
      "fission.autostart pref is locked on this channel which means " +
        "we don't need to run the following tests"
    );
    return;
  }

  const win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  try {
    const browser = win.gBrowser.selectedTab.linkedBrowser;

    BrowserTestUtils.loadURI(browser, parentURL);
    await BrowserTestUtils.browserLoaded(browser, false, parentURL);

    async function setupIFrame(id, url) {
      const iframe = content.document.getElementById(id);

      iframe.contentWindow.location = url;
      await new Promise(resolve => {
        iframe.addEventListener("load", resolve, { once: true });
      });

      return iframe.browsingContext;
    }

    async function spawnSelectDeck(index) {
      async function selectDeck(i) {
        const deck = content.document.getElementById("deck");

        deck.setAttribute("selectedIndex", i);
        await new Promise(resolve => {
          content.window.addEventListener("MozAfterPaint", resolve, {
            once: true,
          });
        });
        return deck.selectedIndex;
      }
      await SpecialPowers.spawn(browser, [index], selectDeck);

      await waitForIFrameUpdates();
    }

    const firstIFrame = await SpecialPowers.spawn(
      browser,
      ["first", iframeURL],
      setupIFrame
    );
    const secondIFrame = await SpecialPowers.spawn(
      browser,
      ["second", iframeURL],
      setupIFrame
    );

    await waitForIFrameUpdates();

    await spawnTestStates(
      firstIFrame,
      "target",
      0,
      nsIAccessibleStates.STATE_OFFSCREEN
    );
    // Disable the check for the target element in the unselected pane of the
    // deck, this should be fixed by bug 1578932.
    // Note: As of now we can't use todo in the script transfered into the
    // out-of-process.
    //await spawnTestStates(
    //  secondIFrame,
    //  "target",
    //  nsIAccessibleStates.STATE_OFFSCREEN,
    //  nsIAccessibleStates.STATE_INVISIBLE
    //);

    // Select the second panel.
    await spawnSelectDeck(1);
    await spawnTestStates(
      firstIFrame,
      "target",
      nsIAccessibleStates.STATE_OFFSCREEN,
      nsIAccessibleStates.STATE_INVISIBLE
    );
    await spawnTestStates(
      secondIFrame,
      "target",
      0,
      nsIAccessibleStates.STATE_OFFSCREEN
    );

    // Select the first panel again.
    await spawnSelectDeck(0);
    await spawnTestStates(
      firstIFrame,
      "target",
      0,
      nsIAccessibleStates.STATE_OFFSCREEN
    );
    await spawnTestStates(
      secondIFrame,
      "target",
      nsIAccessibleStates.STATE_OFFSCREEN,
      nsIAccessibleStates.STATE_INVISIBLE
    );
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
