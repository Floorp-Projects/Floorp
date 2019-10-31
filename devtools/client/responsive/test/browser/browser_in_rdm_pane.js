/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify the inRDMPane property is set on a document when that
// document is being viewed in Responsive Design Mode.

const TEST_URL = "http://example.com/";

add_task(async function() {
  const tab = await addTab(TEST_URL);
  const browser = tab.linkedBrowser;

  const browserUIFlags = [false, true];
  for (const flag in browserUIFlags) {
    info("Setting devtools.responsive.browserUI.enabled to " + flag + ".");
    await SpecialPowers.pushPrefEnv({
      set: [["devtools.responsive.browserUI.enabled", flag]],
    });

    // Start RDM on the tab.
    const { ui } = await openRDM(tab);

    const contentBrowser = flag ? browser : ui.getViewportBrowser();

    const contentURL = await ContentTask.spawn(contentBrowser, {}, function() {
      return content.document.URL;
    });
    info("content URL is " + contentURL);

    const contentInRDMPane = await ContentTask.spawn(
      contentBrowser,
      {},
      function() {
        return content.document.inRDMPane;
      }
    );

    ok(
      contentInRDMPane,
      "After RDM is opened, document should have inRDMPane set to true."
    );

    // Leave RDM.
    await closeRDM(tab);

    await SpecialPowers.popPrefEnv();
  }

  await removeTab(tab);
});
