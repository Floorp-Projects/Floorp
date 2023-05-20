/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that calling `showPopup` multiple time does not lead to invalid state.

add_task(async function () {
  const AutocompletePopup = require("resource://devtools/client/shared/autocomplete-popup.js");

  info("Create an autocompletion popup");
  const { doc } = await createHost();
  const input = doc.createElement("input");
  doc.body.appendChild(input);

  const autocompleteOptions = {
    position: "top",
    autoSelect: true,
    useXulWrapper: true,
  };
  const popup = new AutocompletePopup(doc, autocompleteOptions);
  const items = [{ label: "a" }, { label: "b" }, { label: "c" }];
  popup.setItems(items);

  input.focus();

  let onAllEventsReceived = waitForNEvents(popup, "popup-opened", 3);
  // Note that the lack of `await` on those function calls are wanted.
  popup.openPopup(input, 0, 0, 0);
  popup.openPopup(input, 0, 0, 1);
  popup.openPopup(input, 0, 0, 2);
  await onAllEventsReceived;

  ok(popup.isOpen, "popup is open");
  is(
    popup.selectedIndex,
    2,
    "Selected index matches the one that was set last when calling openPopup"
  );

  onAllEventsReceived = waitForNEvents(popup, "popup-opened", 2);
  // Note that the lack of `await` on those function calls are wanted.
  popup.openPopup(input, 0, 0, 1);
  popup.openPopup(input);
  await onAllEventsReceived;

  ok(popup.isOpen, "popup is open");
  is(
    popup.selectedIndex,
    0,
    "First item is selected, as last call to openPopup did not specify an index and autoSelect is true"
  );

  const onPopupClose = popup.once("popup-closed");
  popup.hidePopup();
  await onPopupClose;
});
