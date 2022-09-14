/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGECONTENT =
  "<!DOCTYPE html>" +
  "<html>" +
  "<style>" +
  "html { " +
  "  height: 120vh;" +
  "  overflow-y: scroll;" +
  "}" +
  "</style>" +
  "</html>";

const pageUrl = "data:text/html," + encodeURIComponent(PAGECONTENT);

add_task(async function test() {
  if (window.devicePixelRatio != 2) {
    ok(
      true,
      "Skip this test since this test is supposed to run on HiDPI mode, " +
        "the devixePixelRato on this machine is " +
        window.devicePixelRatio
    );
    return;
  }

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);

  // Scroll the content a bit.
  const originalScrollPosition = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      content.document.scrollingElement.scrollTop = 100;
      return content.document.scrollingElement.scrollTop;
    }
  );

  // Disabling HiDPI mode and check the scroll position.
  SpecialPowers.DOMWindowUtils.setHiDPIMode(false);
  const scrollPosition = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => {
      return content.document.scrollingElement.scrollTop;
    }
  );
  is(
    originalScrollPosition,
    scrollPosition,
    "The scroll position should be kept"
  );
  BrowserTestUtils.removeTab(tab);

  SpecialPowers.DOMWindowUtils.restoreHiDPIMode();
});
