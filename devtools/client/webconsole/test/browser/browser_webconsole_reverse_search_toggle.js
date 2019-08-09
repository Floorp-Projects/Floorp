/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests showing and hiding the reverse search UI.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>Test reverse search toggle`;
const isMacOS = AppConstants.platform === "macosx";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Close the reverse search UI with ESC");
  await openReverseSearch(hud);
  let onReverseSearchUiClose = waitFor(
    () => getReverseSearchElement(hud) === null
  );
  EventUtils.sendKey("ESCAPE");
  await onReverseSearchUiClose;
  ok(true, "Reverse search was closed with the Esc keyboard shortcut");

  if (isMacOS) {
    info("Close the reverse search UI with Ctrl + C on OSX");
    await openReverseSearch(hud);
    onReverseSearchUiClose = waitFor(
      () => getReverseSearchElement(hud) === null
    );
    EventUtils.synthesizeKey("c", { ctrlKey: true });
    await onReverseSearchUiClose;
    ok(true, "Reverse search was closed with the Ctrl + C keyboard shortcut");
  }

  info("Close the reverse search UI with the close button");
  const reverseSearchElement = await openReverseSearch(hud);
  const closeButton = reverseSearchElement.querySelector(
    ".reverse-search-close-button"
  );
  ok(closeButton, "The close button is displayed");
  is(
    closeButton.title,
    `Close (Esc${isMacOS ? " | Ctrl + C" : ""})`,
    "The close button has the expected tooltip"
  );
  onReverseSearchUiClose = waitFor(() => getReverseSearchElement(hud) === null);
  closeButton.click();
  await onReverseSearchUiClose;
  ok(true, "Reverse search was closed by clicking on the close button");

  info("Close the reverse search UI by clicking on the output");
  await openReverseSearch(hud);
  hud.ui.outputNode.querySelector(".jsterm-input-container").click();
  ok(true, "Reverse search was closed by clicking in the output");
});
