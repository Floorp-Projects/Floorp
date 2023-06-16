/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check "Show tabbing order" works as expected when used with the iframe picker

const TEST_URI = `https://example.com/document-builder.sjs?html=
    <button id=top-btn-1>Top level button before iframe</button>
    <iframe src="https://example.org/document-builder.sjs?html=${encodeURIComponent(`
      <button id=iframe-btn-1>in iframe button 1</button>
      <button id=iframe-btn-2>in iframe button 2</button>
    `)}"></iframe>
    <button id=top-btn-2>Top level button after iframe</button>`;

add_task(async () => {
  const env = await addTestTab(TEST_URI);
  const { doc, panel, store, toolbox, win } = env;

  const topLevelFrameHighlighterTestFront = await toolbox.target.getFront(
    "highlighterTest"
  );

  const iframeTarget = toolbox.commands.targetCommand
    .getAllTargets([toolbox.commands.targetCommand.TYPES.FRAME])
    .find(t => t.url.startsWith("https://example.org"));

  // The iframe only has a dedicated target when Fission or EFT is enabled
  const iframeHighlighterTestFront = iframeTarget
    ? await iframeTarget.getFront("highlighterTest")
    : null;

  const topLevelAccessibilityFrontActorID =
    panel.accessibilityProxy.accessibilityFront.actorID;

  const iframeAccessibilityFrontActorID = iframeTarget
    ? (await iframeTarget.getFront("accessibility")).actorID
    : null;

  info(`Click on "Show Tabbing Order" checkbox`);
  const tabbingOrderCheckbox = doc.getElementById(
    "devtools-display-tabbing-order-checkbox"
  );
  tabbingOrderCheckbox.click();
  await waitUntilState(store, state => state.ui.tabbingOrderDisplayed === true);

  is(tabbingOrderCheckbox.checked, true, "Checkbox is checked");
  let tabbingOrderHighlighterData =
    await topLevelFrameHighlighterTestFront.getTabbingOrderHighlighterData(
      topLevelAccessibilityFrontActorID
    );
  if (isFissionEnabled()) {
    // ⚠️ We don't get the highlighter for the <html> node of the iframe when Fission is enabled.
    // This should be fix as part of Bug 1740509.
    is(
      JSON.stringify(tabbingOrderHighlighterData),
      JSON.stringify([`button#top-btn-1 : 1`, `button#top-btn-2 : 4`]),
      "Tabbing order is visible for the top level target after clicking the checkbox"
    );

    const iframeTabingOrderHighlighterData =
      await iframeHighlighterTestFront.getTabbingOrderHighlighterData(
        iframeAccessibilityFrontActorID
      );

    is(
      JSON.stringify(iframeTabingOrderHighlighterData),
      JSON.stringify([`button#iframe-btn-1 : 2`, `button#iframe-btn-2 : 3`]),
      "Tabbing order is visible for the top level target after clicking the checkbox"
    );
  } else {
    is(
      JSON.stringify(tabbingOrderHighlighterData),
      JSON.stringify([
        `button#top-btn-1 : 1`,
        `html : 2`,
        `button#iframe-btn-1 : 3`,
        `button#iframe-btn-2 : 4`,
        `button#top-btn-2 : 5`,
      ]),
      "Tabbing order is visible for the top level target after clicking the checkbox"
    );
  }

  info("Select the iframe in the iframe picker");
  // Get the iframe picker items
  const menuList = toolbox.doc.getElementById("toolbox-frame-menu");

  if (isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    is(
      menuList,
      null,
      "iframe picker does not show remote frames when Fission is enabled and EFT is disabled"
    );
    return;
  }

  const frames = Array.from(menuList.querySelectorAll(".command"));

  let onInitialized = win.once(win.EVENTS.INITIALIZED);
  frames[1].click();
  await onInitialized;
  await waitUntilState(
    store,
    state => state.ui.tabbingOrderDisplayed === false
  );

  is(
    tabbingOrderCheckbox.checked,
    false,
    "Checkbox is unchecked after selecting an iframe"
  );

  tabbingOrderHighlighterData =
    await topLevelFrameHighlighterTestFront.getTabbingOrderHighlighterData(
      topLevelAccessibilityFrontActorID
    );
  is(
    tabbingOrderHighlighterData.length,
    0,
    "Tabbing order is not visible anymore"
  );

  info(
    `Click on "Show Tabbing Order" checkbox and check that highlighter is only displayed for selected frame`
  );
  tabbingOrderCheckbox.click();
  await waitUntilState(store, state => state.ui.tabbingOrderDisplayed === true);

  tabbingOrderHighlighterData =
    await topLevelFrameHighlighterTestFront.getTabbingOrderHighlighterData(
      topLevelAccessibilityFrontActorID
    );
  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    is(
      tabbingOrderHighlighterData.length,
      0,
      "There's no highlighter displayed on the top level target when focused on specific iframe"
    );

    const iframeTabingOrderHighlighterData =
      await iframeHighlighterTestFront.getTabbingOrderHighlighterData(
        iframeAccessibilityFrontActorID
      );

    is(
      JSON.stringify(iframeTabingOrderHighlighterData),
      JSON.stringify([`button#iframe-btn-1 : 1`, `button#iframe-btn-2 : 2`]),
      "Tabbing order has expected data when a specific iframe is selected"
    );
  } else {
    // When Fission/EFT are not enabled, the highlighter is displayed from the top-level
    // target, but only for the iframe
    is(
      JSON.stringify(tabbingOrderHighlighterData),
      JSON.stringify([`button#iframe-btn-1 : 1`, `button#iframe-btn-2 : 2`]),
      "Tabbing order has expected data when a specific iframe is selected"
    );
  }

  info("Select the top level document back");
  onInitialized = win.once(win.EVENTS.INITIALIZED);
  toolbox.doc.querySelector("#toolbox-frame-menu .command").click();
  await onInitialized;

  is(
    tabbingOrderCheckbox.checked,
    false,
    "Checkbox is unchecked after selecting the top level frame"
  );
  await waitUntilState(
    store,
    state => state.ui.tabbingOrderDisplayed === false
  );

  tabbingOrderHighlighterData =
    await topLevelFrameHighlighterTestFront.getTabbingOrderHighlighterData(
      topLevelAccessibilityFrontActorID
    );

  if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
    const iframeTabingOrderHighlighterData =
      await iframeHighlighterTestFront.getTabbingOrderHighlighterData(
        iframeAccessibilityFrontActorID
      );

    is(
      iframeTabingOrderHighlighterData.length,
      0,
      "Highlighter is hidden on the frame after selecting back the top level target"
    );
  }

  await closeTabToolboxAccessibility(env.tab);
});
