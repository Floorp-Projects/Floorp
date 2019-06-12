/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test checks the behavior of text undo (Ctrl-Z, cmd_undo) in regard to
// autofill.

"use strict";

add_task(async function test() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([
    "http://example.com/",
  ]);

  // Search for "ex".  It should autofill to example.com/.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "ex",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  // Type an x.
  EventUtils.synthesizeKey("x");

  // Nothing should have been autofilled.
  await UrlbarTestUtils.promiseSearchComplete(window);
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!details.autofill);
  Assert.equal(gURLBar.value, "exx");
  Assert.equal(gURLBar.selectionStart, "exx".length);
  Assert.equal(gURLBar.selectionEnd, "exx".length);

  // Undo the typed x.
  goDoCommand("cmd_undo");

  // The text should be restored to ex[ample.com/] (with the part in brackets
  // autofilled and selected).
  await UrlbarTestUtils.promiseSearchComplete(window);
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(details.autofill, !UrlbarPrefs.get("quantumbar"));
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  await PlacesUtils.history.clear();
});
