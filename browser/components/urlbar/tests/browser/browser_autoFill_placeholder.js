/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that the autofill placeholder value is autofilled
// correctly.  The placeholder is a string that we immediately autofill when a
// search starts and before its first result arrives in order to prevent text
// flicker in the input.
//
// Because this test specifically checks autofill *before* searches complete, we
// can't use promiseAutocompleteResultPopup() or other helpers that wait for
// searches to complete.  Instead the test uses fireInputEvent() to trigger
// placeholder autofill and then immediately checks autofill status.

"use strict";

add_task(async function init() {
  await cleanUp();
});

add_task(async function origin() {
  await PlacesTestUtils.addVisits("http://example.com/");

  // Do an initial search that triggers autofill so that the placeholder has an
  // initial value.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ex",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  await searchAndCheck("exa", "example.com/");
  await searchAndCheck("EXAM", "EXAMple.com/");
  await searchAndCheck("eXaMp", "eXaMple.com/");
  await searchAndCheck("exampl", "example.com/");

  await cleanUp();
});

add_task(async function tokenAlias() {
  // We have built-in engine aliases that may conflict with the one we choose
  // here in terms of autofill, so be careful and choose a weird alias.
  await SearchTestUtils.installSearchExtension({ keyword: "@__example" });

  // Do an initial search that triggers autofill so that the placeholder has an
  // initial value.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "@__ex",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "@__example ");
  Assert.equal(gURLBar.selectionStart, "@__ex".length);
  Assert.equal(gURLBar.selectionEnd, "@__example ".length);

  await searchAndCheck("@__exa", "@__example ");
  await searchAndCheck("@__EXAM", "@__EXAMple ");
  await searchAndCheck("@__eXaMp", "@__eXaMple ");
  await searchAndCheck("@__exampl", "@__example ");

  await cleanUp();
});

add_task(async function noMatch1() {
  await PlacesTestUtils.addVisits("http://example.com/");

  // Do an initial search that triggers autofill so that the placeholder has an
  // initial value.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ex",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  // Search with a string that does not match the placeholder.  Placeholder
  // autofill shouldn't happen.
  gURLBar.value = "moz";
  UrlbarTestUtils.fireInputEvent(window);
  Assert.equal(gURLBar.value, "moz");
  Assert.equal(gURLBar.selectionStart, "moz".length);
  Assert.equal(gURLBar.selectionEnd, "moz".length);
  await UrlbarTestUtils.promiseSearchComplete(window);

  // Search for "ex" again.  It should be autofilled.  Placeholder autofill
  // won't happen.  It's not important for this test to check that.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ex",
    fireInputEvent: true,
  });
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  // Placeholder autofill should work again for example.com searches.
  await searchAndCheck("exa", "example.com/");
  await searchAndCheck("EXAM", "EXAMple.com/");
  await searchAndCheck("eXaMp", "eXaMple.com/");
  await searchAndCheck("exampl", "example.com/");

  await cleanUp();
});

add_task(async function noMatch2() {
  await PlacesTestUtils.addVisits([
    "http://mozilla.org/",
    "http://example.com/",
  ]);

  // Do an initial search that triggers autofill so that the placeholder has an
  // initial value.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "moz",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "mozilla.org/");
  Assert.equal(gURLBar.selectionStart, "moz".length);
  Assert.equal(gURLBar.selectionEnd, "mozilla.org/".length);

  // Search with a string that does not match the placeholder but does trigger
  // autofill.  Placeholder autofill shouldn't happen.
  gURLBar.value = "ex";
  UrlbarTestUtils.fireInputEvent(window);
  Assert.equal(gURLBar.value, "ex");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "ex".length);
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "ex".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  // Do some searches that should trigger placeholder autofill.
  await searchAndCheck("exa", "example.com/");
  await searchAndCheck("EXAm", "EXAmple.com/");

  // Search for "moz" again.  It should be autofilled.  Placeholder autofill
  // shouldn't happen.
  gURLBar.value = "moz";
  UrlbarTestUtils.fireInputEvent(window);
  Assert.equal(gURLBar.value, "moz");
  Assert.equal(gURLBar.selectionStart, "moz".length);
  Assert.equal(gURLBar.selectionEnd, "moz".length);
  await UrlbarTestUtils.promiseSearchComplete(window);
  Assert.equal(gURLBar.value, "mozilla.org/");
  Assert.equal(gURLBar.selectionStart, "moz".length);
  Assert.equal(gURLBar.selectionEnd, "mozilla.org/".length);

  // Do some searches that should trigger placeholder autofill.
  await searchAndCheck("mozi", "mozilla.org/");
  await searchAndCheck("MOZil", "MOZilla.org/");

  await cleanUp();
});

add_task(async function clear_placeholder_for_keyword_or_alias() {
  info("Clear the autofill placeholder if a keyword is typed");
  await PlacesTestUtils.addVisits("https://example.com/");
  await PlacesUtils.keywords.insert({
    keyword: "ex",
    url: "https://somekeyword.com/",
  });
  await SearchTestUtils.installSearchExtension({ keyword: "exam" });
  registerCleanupFunction(async function() {
    await PlacesUtils.keywords.remove("ex");
  });

  // Do an initial search that triggers autofill so that the placeholder has an
  // initial value.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "e",
    fireInputEvent: true,
  });
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "example.com/");
  Assert.equal(gURLBar.selectionStart, "e".length);
  Assert.equal(gURLBar.selectionEnd, "example.com/".length);

  // The values are initially autofilled on input, then the placeholder is
  // removed when the first non-autofill result arrives.

  // Matches the keyword.
  await searchAndCheck("ex", "example.com/", "ex");
  await searchAndCheck("EXA", "EXAmple.com/", "EXAmple.com/");
  // Matches the alias.

  await searchAndCheck("eXaM", "eXaMple.com/", "eXaMple.com/");
  await searchAndCheck("examp", "example.com/", "example.com/");

  await cleanUp();
});

add_task(async function clear_placeholder_for_uri_fragment() {
  info(
    "Clear the autofill placeholder if the value has uri fragment that does not match with placeholder"
  );
  await PlacesTestUtils.addVisits("https://example.com/#TEST");

  const testData = [
    {
      input: "https://example.com/#T",
      autofilled: "https://example.com/#TEST",
      invalidInput: "https://example.com/#t",
    },
    {
      input: "example.com/#T",
      autofilled: "example.com/#TEST",
      invalidInput: "example.com/#t",
    },
    {
      input: "example.com/#T",
      autofilled: "example.com/#TEST",
      invalidInput: "example.com/",
    },
  ];

  for (const { input, autofilled, invalidInput } of testData) {
    // Do an initial search that triggers autofill so that the placeholder has an
    // initial value.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: input,
      fireInputEvent: true,
    });

    // Autofilled by placeholder.
    await searchAndCheck(input, autofilled);

    // Not autofilled and clear the placeholder because the URI fragment does
    // not match.
    await searchAndCheck(invalidInput, invalidInput);

    // Not autofilled since placeholder is already cleared.
    await searchAndCheck(input, input);
  }

  await cleanUp();
});

add_task(async function clear_placeholder_for_deep_path() {
  info("Check if not autofill if the value expresses parent directory");
  await PlacesTestUtils.addVisits("http://example.com/shallow/deep/file");

  const testData = [
    {
      input: "example.com/s",
      autofilled: "example.com/shallow/",
      invalidInput: "example.com/",
    },
    {
      input: "example.com/shallow/d",
      autofilled: "example.com/shallow/deep/",
      invalidInput: "example.com/shallow/",
    },
    {
      input: "example.com/shallow/deep/f",
      autofilled: "example.com/shallow/deep/file",
      invalidInput: "example.com/shallow/deep/",
    },
  ];

  for (const { input, autofilled, invalidInput } of testData) {
    // Do an initial search that triggers autofill so that the placeholder has an
    // initial value.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: input,
      fireInputEvent: true,
    });

    // Should be autofilled.
    await searchAndCheck(input, autofilled);

    // Should not be autofilled.
    await searchAndCheck(invalidInput, invalidInput);
  }

  await cleanUp();
});

async function searchAndCheck(
  searchString,
  expectedAutofillValue,
  onCompleteValue = ""
) {
  gURLBar.value = searchString;

  // Placeholder autofill is done on input, so fire an input event.  As the
  // comment at the top of this file says, we can't use
  // promiseAutocompleteResultPopup() or other helpers that wait for searches to
  // complete because we are specifically checking autofill before the search
  // completes.
  UrlbarTestUtils.fireInputEvent(window);

  // Check the input value and selection immediately, before waiting on the
  // search to complete.
  Assert.equal(gURLBar.value, expectedAutofillValue);
  Assert.equal(gURLBar.selectionStart, searchString.length);
  Assert.equal(gURLBar.selectionEnd, expectedAutofillValue.length);

  await UrlbarTestUtils.promiseSearchComplete(window);

  if (onCompleteValue) {
    // Check the final value after the results arrived.
    Assert.equal(gURLBar.value, onCompleteValue);
    Assert.equal(gURLBar.selectionStart, searchString.length);
    Assert.equal(gURLBar.selectionEnd, onCompleteValue.length);
  }
}

async function cleanUp() {
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
