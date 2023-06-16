/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify the inRDMPane property is set on a document when that
// document is being viewed in Responsive Design Mode.

const TEST_URL = "http://example.com/";

addRDMTask(TEST_URL, async function ({ ui }) {
  const viewportBrowser = ui.getViewportBrowser();

  const contentURL = await SpecialPowers.spawn(
    viewportBrowser,
    [],
    () => content.document.URL
  );
  info("content URL is " + contentURL);

  const contentInRDMPane = await SpecialPowers.spawn(
    viewportBrowser,
    [],
    () => docShell.browsingContext.inRDMPane
  );

  ok(
    contentInRDMPane,
    "After RDM is opened, document should have inRDMPane set to true."
  );
});
