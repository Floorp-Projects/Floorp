/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check "Show tabbing order" works as expected

const TEST_URI = `https://example.com/document-builder.sjs?html=
    <button id=top-btn-1>Top level button before iframe</button>
    <iframe src="https://example.org/document-builder.sjs?html=${encodeURIComponent(`
      <button id=iframe-org-btn-1>in iframe button 1</button>
      <button id=iframe-org-btn-2>in iframe button 2</button>
    `)}"></iframe>
    <button id=top-btn-2>Top level button after iframe</button>
    <iframe src="https://example.net/document-builder.sjs?html=${encodeURIComponent(`
      <button id=iframe-net-btn-1>in iframe button 1</button>
    `)}"></iframe>`;

add_task(async () => {
  const { doc, store, tab, toolbox } = await addTestTab(TEST_URI);

  const topLevelFrameHighlighterTestFront = await toolbox.target.getFront(
    "highlighterTest"
  );

  const frameTargets = toolbox.commands.targetCommand.getAllTargets([
    toolbox.commands.targetCommand.TYPES.FRAME,
  ]);
  const orgIframeTarget = frameTargets.find(t =>
    t.url.startsWith("https://example.org")
  );
  const netIframeTarget = frameTargets.find(t =>
    t.url.startsWith("https://example.net")
  );

  // The iframe only has a dedicated target when Fission or EFT is enabled
  const orgIframeHighlighterTestFront = orgIframeTarget
    ? await orgIframeTarget.getFront("highlighterTest")
    : null;
  const netIframeHighlighterTestFront = netIframeTarget
    ? await netIframeTarget.getFront("highlighterTest")
    : null;

  let tabbingOrderHighlighterData =
    await topLevelFrameHighlighterTestFront.getTabbingOrderHighlighterData();
  is(
    tabbingOrderHighlighterData.length,
    0,
    "Tabbing order is not visible at first"
  );

  info(`Click on "Show Tabbing Order" checkbox`);
  const tabbingOrderCheckbox = doc.getElementById(
    "devtools-display-tabbing-order-checkbox"
  );
  tabbingOrderCheckbox.click();

  await waitUntilState(store, state => state.ui.tabbingOrderDisplayed === true);

  is(tabbingOrderCheckbox.checked, true, "Checkbox is checked");
  tabbingOrderHighlighterData =
    await topLevelFrameHighlighterTestFront.getTabbingOrderHighlighterData();
  if (isFissionEnabled()) {
    // ⚠️ We don't get the highlighter for the <html> node of the iframe when Fission is enabled.
    // This should be fix as part of Bug 1740509.
    is(
      JSON.stringify(tabbingOrderHighlighterData),
      JSON.stringify([`button#top-btn-1 : 1`, `button#top-btn-2 : 4`]),
      "Tabbing order is visible for the top level target after clicking the checkbox"
    );

    const orgIframeTabingOrderHighlighterData =
      await orgIframeHighlighterTestFront.getTabbingOrderHighlighterData();
    is(
      JSON.stringify(orgIframeTabingOrderHighlighterData),
      JSON.stringify([
        `button#iframe-org-btn-1 : 2`,
        `button#iframe-org-btn-2 : 3`,
      ]),
      "Tabbing order is visible for the org iframe after clicking the checkbox"
    );

    const netIframeTabingOrderHighlighterData =
      await netIframeHighlighterTestFront.getTabbingOrderHighlighterData();
    is(
      JSON.stringify(netIframeTabingOrderHighlighterData),
      JSON.stringify([`button#iframe-net-btn-1 : 5`]),
      "Tabbing order is visible for the net iframe after clicking the checkbox"
    );
  } else {
    is(
      JSON.stringify(tabbingOrderHighlighterData),
      JSON.stringify([
        `button#top-btn-1 : 1`,
        `html : 2`,
        `button#iframe-org-btn-1 : 3`,
        `button#iframe-org-btn-2 : 4`,
        `button#top-btn-2 : 5`,
        `html : 6`,
        `button#iframe-net-btn-1 : 7`,
      ]),
      "Tabbing order is visible for the top level target after clicking the checkbox"
    );
  }

  info(`Clicking on the checkbox again hides the highlighter`);
  tabbingOrderCheckbox.click();
  await waitUntilState(
    store,
    state => state.ui.tabbingOrderDisplayed === false
  );

  is(tabbingOrderCheckbox.checked, false, "Checkbox is unchecked");
  tabbingOrderHighlighterData =
    await topLevelFrameHighlighterTestFront.getTabbingOrderHighlighterData();
  is(
    tabbingOrderHighlighterData.length,
    0,
    "Tabbing order is not visible anymore after unchecking the checkbox"
  );

  if (isFissionEnabled()) {
    const orgIframeTabingOrderHighlighterData =
      await orgIframeHighlighterTestFront.getTabbingOrderHighlighterData();
    is(
      orgIframeTabingOrderHighlighterData.length,
      0,
      "Tabbing order is also hidden on the org iframe target"
    );
    const netIframeTabingOrderHighlighterData =
      await netIframeHighlighterTestFront.getTabbingOrderHighlighterData();
    is(
      netIframeTabingOrderHighlighterData.length,
      0,
      "Tabbing order is also hidden on the net iframe target"
    );
  }

  await closeTabToolboxAccessibility(tab);
});
