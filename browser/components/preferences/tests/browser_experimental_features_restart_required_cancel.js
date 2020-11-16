/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testInfiniteCancelLoop() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.experimental", true]],
  });

  const server = new DefinitionServer();
  server.addDefinition({
    id: "test-featureA",
    isPublic: true,
    preference: "test.feature.a",
    restartRequired: true,
  });
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `about:preferences?definitionsUrl=${encodeURIComponent(
      server.definitionsUrl
    )}#paneExperimental`
  );
  let doc = gBrowser.contentDocument;

  // Trigger and cancel a feature that has restart-required=true to make
  // sure that we don't enter an infinite loop of prompts.
  let featureCheckbox = doc.getElementById("test-featureA");
  ok(featureCheckbox, "Checkbox should exist");
  let newWindowPromise = BrowserTestUtils.domWindowOpened();
  featureCheckbox.click();

  let restartWin = await newWindowPromise;
  let dialog = await TestUtils.waitForCondition(() =>
    restartWin.document.querySelector(`dialog`)
  );
  let cancelButton = dialog.shadowRoot.querySelector(`[dlgtype="cancel"]`);
  ok(cancelButton, "Cancel button should exist in dialog");
  let windowClosedPromise = BrowserTestUtils.domWindowClosed(restartWin);
  cancelButton.click();
  await windowClosedPromise;

  // If a new prompt is opened then the test should fail since
  // the prompt window will have leaked.

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
