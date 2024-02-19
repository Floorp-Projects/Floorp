/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  // Create a tab that loads a system font.
  const CROSS_ORIGIN_DOMAIN = "https://example.com";
  const TARGET_URL = `${CROSS_ORIGIN_DOMAIN}/browser/gfx/tests/browser/file_native_font_cache_macos.html`;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TARGET_URL },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        // Capture a snapshot of the tab, which will load the system font in the
        // parent process.
        const TARGET_WIDTH = 200;
        const TARGET_HEIGHT = 100;

        const rect = new content.window.DOMRect(
          0,
          0,
          TARGET_WIDTH,
          TARGET_HEIGHT
        );
        await SpecialPowers.snapshotContext(content.window, rect, "white");
      });
    }
  );

  // Now create a tab that shows the memory reporter.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:memory" },
    async browser => {
      // Click the "Measure" button.
      await SpecialPowers.spawn(browser, [], () => {
        let measureButton = content.document.getElementById("measureButton");
        measureButton.click();
      });

      // Copy the page text and check for an expected start with string.
      let copiedText = await new Promise(resolve => {
        const REPORT_TIMEOUT_MS = 15 * 1e3;
        const EXPECTED_START_WITH = "Main Process";
        let mostRecentTextOnClipboard = "";

        SimpleTest.waitForClipboard(
          textOnClipboard => {
            mostRecentTextOnClipboard = textOnClipboard;
            const gotExpected = textOnClipboard.startsWith(EXPECTED_START_WITH);
            if (!gotExpected) {
              // Try copying again.
              EventUtils.synthesizeKey("A", { accelKey: true });
              EventUtils.synthesizeKey("C", { accelKey: true });
            }
            return gotExpected;
          },
          () => {
            EventUtils.synthesizeKey("A", { accelKey: true });
            EventUtils.synthesizeKey("C", { accelKey: true });
          },
          () => {
            resolve(mostRecentTextOnClipboard);
          },
          () => {
            info(`Didn't find expected text within ${REPORT_TIMEOUT_MS}ms.`);
            dump("*******ACTUAL*******\n");
            dump("<<<" + mostRecentTextOnClipboard + ">>>\n");
            dump("********************\n");
            resolve("");
          },
          "text/plain",
          REPORT_TIMEOUT_MS
        );
      });

      isnot(copiedText, "", "Got some text from clipboard.");

      // Search the copied text for our desired pattern. Initially, check for
      // a line with "native-font-resource-mac". If that exists, ensure that it
      // has less than a maximum MB. If that doesn't exist, check instead for
      // a line with "gfx" before the "Other Measurements" section. If that
      // exists, it is tested against the same MB limit. If it doesn't exist,
      // that is an indication that "gfx" doesn't occur in the first section
      // "Explicit Allocations', and therefore isn't holding memory at all.
      const MB_EXCLUSIVE_MAX = 20;
      const nfrm_line = /^.*?(\d+)\.\d+ MB.*-- native-font-resource-mac/m;
      const nfrm_match = nfrm_line.exec(copiedText);
      if (nfrm_match) {
        const nfrm_mb = nfrm_match[1];
        Assert.less(
          nfrm_mb,
          MB_EXCLUSIVE_MAX,
          `native-font-resource-mac ${nfrm_mb} MB should be less than ${MB_EXCLUSIVE_MAX} MB.`
        );
      } else {
        // Figure out where the "Other Measurements" section begins.
        const om_line = /^Other Measurements$/m;
        const om_match = om_line.exec(copiedText);

        // Find the first gfx line, and if it occurs before the "Other
        // Measurements" section, check its size.
        const gfx_line = /^.*?(\d+)\.\d+ MB.*-- gfx/m;
        const gfx_match = gfx_line.exec(copiedText);
        if (gfx_match && gfx_match.index < om_match.index) {
          const gfx_mb = gfx_match[1];
          Assert.less(
            gfx_mb,
            MB_EXCLUSIVE_MAX,
            `Explicit Allocations gfx ${gfx_mb} MB should be less than ${MB_EXCLUSIVE_MAX} MB.`
          );
        } else {
          ok(true, "Explicit Allocations gfx is not listed.");
        }
      }
    }
  );
});
