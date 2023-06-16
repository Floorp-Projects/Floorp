/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the DOM panel works as expected when a specific frame is selected in the
// iframe picker.

const TEST_URL = `https://example.com/document-builder.sjs?html=
  <h1>top_level</h1>
  <iframe src="https://example.org/document-builder.sjs?html=in_iframe"></iframe>`;

add_task(async function () {
  const { panel } = await addTestTab(TEST_URL);
  const toolbox = panel._toolbox;

  info("Wait until the iframe picker button is visible");
  try {
    await waitFor(() => toolbox.doc.getElementById("command-button-frames"));
  } catch (e) {
    if (isFissionEnabled() && !isEveryFrameTargetEnabled()) {
      ok(
        true,
        "Remote frames are not displayed in iframe picker if Fission is enabled but EFT is not"
      );
      return;
    }
    throw e;
  }

  info("Check `document` property when no specific frame is focused");
  let documentPropertyValue = getDocumentPropertyValue(panel);

  ok(
    documentPropertyValue.startsWith("HTMLDocument https://example.com"),
    `Got expected "document" value (${documentPropertyValue})`
  );

  info(
    "Select the frame in the iframe picker and check that the document property is updated"
  );
  // Wait for the DOM panel to refresh.
  const store = getReduxStoreFromPanel(panel);
  let onPropertiesFetched = waitForDispatch(store, "FETCH_PROPERTIES");

  const exampleOrgFrame = toolbox.doc.querySelector(
    "#toolbox-frame-menu .menuitem:last-child .command"
  );

  exampleOrgFrame.click();
  await onPropertiesFetched;

  documentPropertyValue = getDocumentPropertyValue(panel);
  ok(
    documentPropertyValue.startsWith("HTMLDocument https://example.org"),
    `Got expected "document" value (${documentPropertyValue})`
  );

  info(
    "Select the top-level frame and check that the document property is updated"
  );
  onPropertiesFetched = waitForDispatch(store, "FETCH_PROPERTIES");

  const exampleComFrame = toolbox.doc.querySelector(
    "#toolbox-frame-menu .menuitem:first-child .command"
  );
  exampleComFrame.click();
  await onPropertiesFetched;

  documentPropertyValue = getDocumentPropertyValue(panel);
  ok(
    documentPropertyValue.startsWith("HTMLDocument https://example.com"),
    `Got expected "document" value (${documentPropertyValue})`
  );
});

function getDocumentPropertyValue(panel) {
  return getRowByLabel(panel, "document").querySelector("td.treeValueCell")
    .textContent;
}
