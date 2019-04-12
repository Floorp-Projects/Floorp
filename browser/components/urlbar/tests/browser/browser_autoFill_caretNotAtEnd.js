/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function noAutofillWhenCaretNotAtEnd() {
  gURLBar.focus();

  // Add a visit that can be autofilled.
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/",
  }]);

  // Fill the input with xample.
  gURLBar.inputField.value = "xample";

  // Move the caret to the beginning and type e.
  gURLBar.selectionStart = 0;
  gURLBar.selectionEnd = 0;
  EventUtils.sendString("e");

  // Check the first result and input.
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!result.autofill, "The first result should not be autofill");

  Assert.equal(gURLBar.value, "example");
  Assert.equal(gURLBar.selectionStart, 1);
  Assert.equal(gURLBar.selectionEnd, 1);

  await PlacesUtils.history.clear();
});
