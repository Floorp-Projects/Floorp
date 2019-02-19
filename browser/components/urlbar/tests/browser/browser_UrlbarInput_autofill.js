/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the autofill functionality of UrlbarInput.
 */

"use strict";

add_task(async function setValueFromResult() {
  gURLBar.setValueFromResult({
    autofill: {
      value: "foobar",
      selectionStart: "foo".length,
      selectionEnd: "foobar".length,
    },
    type: UrlbarUtils.RESULT_TYPE.URL,
  });
  Assert.equal(gURLBar.value, "foobar",
    "The input value should be correct");
  Assert.equal(gURLBar.selectionStart, "foo".length,
    "The start of the selection should be correct");
  Assert.equal(gURLBar.selectionEnd, "foobar".length,
    "The end of the selection should be correct");
});

add_task(async function noAutofillWhenCaretNotAtEnd() {
  gURLBar.focus();

  // Autofill is disabled when the new search starts with the previous search,
  // so to make sure that doesn't mess up this test, trigger a search now.
  EventUtils.sendString("blah");

  // Add a visit that can be autofilled.
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/",
  }]);

  // Fill the input with xample.com.
  gURLBar.inputField.value = "xample.com";

  // Move the caret to the beginning and type e.
  gURLBar.selectionStart = 0;
  gURLBar.selectionEnd = 0;
  EventUtils.sendString("e");

  // Get the first result.
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!result.autofill, "The first result should not be autofill");

  await UrlbarTestUtils.promisePopupClose(window);
  await PlacesUtils.history.clear();
});
