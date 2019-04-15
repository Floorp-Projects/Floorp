/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that autofilling the first result of a new search works
// correctly: autofill happens when it should and doesn't when it shouldn't.

"use strict";

add_task(async function init() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([
    "http://example.com/",
  ]);

  // Disable placeholder completion.  The point of this test is to make sure the
  // first result is autofilled (or not) correctly.  Autofilling the placeholder
  // before the search starts interferes with that.
  gURLBar._enableAutofillPlaceholder = false;
  registerCleanupFunction(async () => {
    gURLBar._enableAutofillPlaceholder = true;
  });
});

// The first result should be autofilled when all conditions are met.  This also
// does a sanity check to make sure that placeholder autofill is correctly
// disabled, which is helpful for all tasks here and is why this one is first.
add_task(async function successfulAutofill() {
  // Do a simple search that should autofill.  This will also set up the
  // autofill placeholder string, which next we make sure is *not* autofilled.
  await doInitialAutofillSearch();

  // As a sanity check, do another search to make sure the placeholder is *not*
  // autofilled.  Make sure it's not autofilled by checking the input value and
  // selection *before* the search completes.  If placeholder autofill was not
  // correctly disabled, then these assertions will fail.

  gURLBar.value = "exa";
  UrlbarTestUtils.fireInputEvent(window);

  // before the search completes: no autofill
  Assert.equal(gURLBar.value, "exa");
  Assert.equal(gURLBar.selectionStart, "exa".length);
  Assert.equal(gURLBar.selectionEnd, "exa".length);

  await UrlbarTestUtils.promiseSearchComplete(window);

  // after the search completes: successful autofill
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "exa".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);
});

// The first result should not be autofilled when it's not an autofill result.
add_task(async function firstResultNotAutofill() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "foo",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!details.autofill);
  Assert.equal(gURLBar.value, "foo");
  Assert.equal(gURLBar.selectionStart, "foo".length);
  Assert.equal(gURLBar.selectionEnd, "foo".length);
});

// The first result should *not* be autofilled when the placeholder is not
// selected, the selection is empty, and the caret is *not* at the end of the
// search string.
add_task(async function caretNotAtEndOfSearchString() {
  // To set up the placeholder, do an initial search that triggers autofill.
  await doInitialAutofillSearch();

  // Now do another search but set the caret to somewhere else besides the end
  // of the new search string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exam",
    selectionStart: "exa".length,
    selectionEnd: "exa".length,
  });

  // The first result should be an autofill result, but it should not have been
  // autofilled.
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "exam");
  Assert.equal(gURLBar.selectionStart, "exa".length);
  Assert.equal(gURLBar.selectionEnd, "exa".length);

  await cleanUp();
});

// The first result should *not* be autofilled when the placeholder is not
// selected, the selection is *not* empty, and the caret is at the end of the
// search string.
add_task(async function selectionNotEmpty() {
  // To set up the placeholder, do an initial search that triggers autofill.
  await doInitialAutofillSearch();

  // Now do another search.  Set the selection end at the end of the search
  // string, but make the selection non-empty.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exam",
    selectionStart: "exa".length,
    selectionEnd: "exam".length,
  });

  // The first result should be an autofill result, but it should not have been
  // autofilled.
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "exam");
  Assert.equal(gURLBar.selectionStart, "exa".length);
  Assert.equal(gURLBar.selectionEnd, "exam".length);

  await cleanUp();
});

// The first result should be autofilled when the placeholder is not selected,
// the selection is empty, and the caret is at the end of the search string.
add_task(async function successfulAutofillAfterSettingPlaceholder() {
  // To set up the placeholder, do an initial search that triggers autofill.
  await doInitialAutofillSearch();

  // Now do another search.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exam",
    selectionStart: "exam".length,
    selectionEnd: "exam".length,
  });

  // It should be autofilled.
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "exam".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  await cleanUp();
});

// The first result should be autofilled when the placeholder *is* selected --
// more precisely, when the portion of the placeholder after the new search
// string is selected.
add_task(async function successfulAutofillPlaceholderSelected() {
  // To set up the placeholder, do an initial search that triggers autofill.
  await doInitialAutofillSearch();

  // Now do another search and select the portion of the placeholder after the
  // new search string.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exam",
    selectionStart: "exam".length,
    selectionEnd: "example.com/".length,
  });

  // It should be autofilled.
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "exam".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  await cleanUp();
});

async function doInitialAutofillSearch() {
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
}

async function cleanUp() {
  // In some cases above, a test task searches for "exam" at the end, and then
  // the next task searches for "ex".  Autofill results will not be allowed in
  // the next task in that case since the old search string starts with the new
  // search string.  To prevent one task from interfering with the next, do a
  // search that changes the search string.  Also close the popup while we're
  // here, although that's not really necessary.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "reset last search string",
  });
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
}
