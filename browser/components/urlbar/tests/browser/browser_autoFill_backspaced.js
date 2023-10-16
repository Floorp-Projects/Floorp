/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test ensures that backspacing autoFilled values still allows to
 * confirm the remaining value.
 */

"use strict";

async function test_autocomplete(data) {
  let { desc, typed, autofilled, modified, keys, type, onAutoFill } = data;
  info(desc);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: typed,
    fireInputEvent: true,
  });
  Assert.equal(gURLBar.value, autofilled, "autofilled value is as expected");
  if (onAutoFill) {
    onAutoFill();
  }

  info("Synthesizing keys");
  for (let key of keys) {
    let args = Array.isArray(key) ? key : [key];
    EventUtils.synthesizeKey(...args);
  }
  await UrlbarTestUtils.promiseSearchComplete(window);

  Assert.equal(
    gURLBar.value,
    UrlbarTestUtils.trimURL(modified),
    "backspaced value is as expected"
  );

  Assert.greater(
    UrlbarTestUtils.getResultCount(window),
    0,
    "Should get at least 1 result"
  );

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  Assert.equal(result.type, type, "Should have the correct result type");

  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.blur();
}

add_task(async function () {
  registerCleanupFunction(async function () {
    Services.prefs.clearUserPref("browser.urlbar.autoFill");
    Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
    gURLBar.handleRevert();
    await PlacesUtils.history.clear();
  });
  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

  await PlacesTestUtils.addVisits([
    "http://example.com/",
    "http://example.com/foo",
  ]);
  // Bookmark the page so it ignores autofill threshold and doesn't risk to
  // not be autofilled.
  let bm = await PlacesUtils.bookmarks.insert({
    url: "http://example.com/",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.remove(bm);
  });

  await test_autocomplete({
    desc: "DELETE the autofilled part should search",
    typed: "exam",
    autofilled: "example.com/",
    modified: "exam",
    keys: ["KEY_Delete"],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
  });
  await test_autocomplete({
    desc: "DELETE the final slash should visit",
    typed: "example.com",
    autofilled: "example.com/",
    modified: "example.com",
    keys: ["KEY_Delete"],
    type: UrlbarUtils.RESULT_TYPE.URL,
  });

  await test_autocomplete({
    desc: "BACK_SPACE the autofilled part should search",
    typed: "exam",
    autofilled: "example.com/",
    modified: "exam",
    keys: ["KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
  });
  await test_autocomplete({
    desc: "BACK_SPACE the final slash should visit",
    typed: "example.com",
    autofilled: "example.com/",
    modified: "example.com",
    keys: ["KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.URL,
  });

  await test_autocomplete({
    desc: "DELETE the autofilled part, then BACK_SPACE, should search",
    typed: "exam",
    autofilled: "example.com/",
    modified: "exa",
    keys: ["KEY_Delete", "KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
  });
  await test_autocomplete({
    desc: "DELETE the final slash, then BACK_SPACE, should search",
    typed: "example.com",
    autofilled: "example.com/",
    modified: "example.co",
    keys: ["KEY_Delete", "KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.URL,
  });

  await test_autocomplete({
    desc: "BACK_SPACE the autofilled part, then BACK_SPACE, should search",
    typed: "exam",
    autofilled: "example.com/",
    modified: "exa",
    keys: ["KEY_Backspace", "KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
  });
  await test_autocomplete({
    desc: "BACK_SPACE the final slash, then BACK_SPACE, should search",
    typed: "example.com",
    autofilled: "example.com/",
    modified: "example.co",
    keys: ["KEY_Backspace", "KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.URL,
  });

  await test_autocomplete({
    desc: "BACK_SPACE after blur should search",
    typed: "ex",
    autofilled: "example.com/",
    modified: "e",
    keys: ["KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
    onAutoFill: () => {
      gURLBar.blur();
      gURLBar.focus();
      Assert.equal(
        gURLBar.selectionStart,
        gURLBar.value.length,
        "blur/focus should not change selection"
      );
      Assert.equal(
        gURLBar.selectionEnd,
        gURLBar.value.length,
        "blur/focus should not change selection"
      );
    },
  });
  await test_autocomplete({
    desc: "DELETE after blur should search",
    typed: "ex",
    autofilled: "example.com/",
    modified: "e",
    keys: ["KEY_ArrowLeft", "KEY_Delete"],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
    onAutoFill: () => {
      gURLBar.blur();
      gURLBar.focus();
      Assert.equal(
        gURLBar.selectionStart,
        gURLBar.value.length,
        "blur/focus should not change selection"
      );
      Assert.equal(
        gURLBar.selectionEnd,
        gURLBar.value.length,
        "blur/focus should not change selection"
      );
    },
  });
  await test_autocomplete({
    desc: "double BACK_SPACE after blur should search",
    typed: "exa",
    autofilled: "example.com/",
    modified: "e",
    keys: ["KEY_Backspace", "KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
    onAutoFill: () => {
      gURLBar.blur();
      gURLBar.focus();
      Assert.equal(
        gURLBar.selectionStart,
        gURLBar.value.length,
        "blur/focus should not change selection"
      );
      Assert.equal(
        gURLBar.selectionEnd,
        gURLBar.value.length,
        "blur/focus should not change selection"
      );
    },
  });

  await test_autocomplete({
    desc: "Right arrow key and then backspace should delete the backslash and not re-trigger autofill",
    typed: "ex",
    autofilled: "example.com/",
    modified: "example.com",
    keys: ["KEY_ArrowRight", "KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.URL,
  });

  await test_autocomplete({
    desc: "Right arrow key, selecting the last few characters using the keyboard, and then backspace should delete the characters and not re-trigger autofill",
    typed: "ex",
    autofilled: "example.com/",
    modified: "example.c",
    keys: [
      "KEY_ArrowRight",
      ["KEY_ArrowLeft", { shiftKey: true }],
      ["KEY_ArrowLeft", { shiftKey: true }],
      ["KEY_ArrowLeft", { shiftKey: true }],
      "KEY_Backspace",
    ],
    type: UrlbarUtils.RESULT_TYPE.SEARCH,
  });

  await test_autocomplete({
    desc: "End and then backspace should delete the backslash and not re-trigger autofill",
    typed: "ex",
    autofilled: "example.com/",
    modified: "example.com",
    keys: [
      AppConstants.platform == "macosx"
        ? ["KEY_ArrowRight", { metaKey: true }]
        : "KEY_End",
      "KEY_Backspace",
    ],
    type: UrlbarUtils.RESULT_TYPE.URL,
  });

  await test_autocomplete({
    desc: "Clicking in the input after the text and then backspace should delete the backslash and not re-trigger autofill",
    typed: "ex",
    autofilled: "example.com/",
    modified: "example.com",
    keys: ["KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.URL,
    onAutoFill: () => {
      // This assumes that the center of the input is to the right of the end
      // of the text, so the caret is placed at the end of the text on click.
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    },
  });

  await test_autocomplete({
    desc: "Selecting the next result and then backspace should delete the last character and not re-trigger autofill",
    typed: "ex",
    autofilled: "example.com/",
    modified: "http://example.com/fo",
    keys: ["KEY_ArrowDown", "KEY_Backspace"],
    type: UrlbarUtils.RESULT_TYPE.URL,
  });

  await PlacesUtils.history.clear();
});
