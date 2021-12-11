/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test handling whitespace chars such as "\n”.

const TEST_DATA = [
  {
    input: "this is a\ntest",
    expected: {
      urlbar: "this is a test",
      autocomplete: "this is a test",
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
    },
  },
  {
    input: "this is    a\n\ttest",
    expected: {
      urlbar: "this is    a  test",
      autocomplete: "this is    a  test",
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
    },
  },
  {
    input: "http:\n//\nexample.\ncom",
    expected: {
      urlbar: "http://example.com",
      autocomplete: "http://example.com/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "htp:example.\ncom",
    expected: {
      urlbar: "htp:example.com",
      autocomplete: "http://example.com/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "example.\ncom",
    expected: {
      urlbar: "example.com",
      autocomplete: "http://example.com/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "http://example.com/foo       bar/",
    expected: {
      urlbar: "http://example.com/foo       bar/",
      autocomplete: "http://example.com/foo       bar/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "javasc\nript:\nalert(1)",
    expected: {
      urlbar: "alert(1)",
      autocomplete: "alert(1)",
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
    },
  },
  {
    input: "a\nb\nc",
    expected: {
      urlbar: "a b c",
      autocomplete: "a b c",
      type: UrlbarUtils.RESULT_TYPE.SEARCH,
    },
  },
  {
    input: "lo\ncal\nhost",
    expected: {
      urlbar: "localhost",
      autocomplete: "http://localhost/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "data:text/html,<iframe\n src='example\n.com'>\n</iframe>",
    expected: {
      urlbar: "data:text/html,<iframe src='example.com'></iframe>",
      autocomplete: "data:text/html,<iframe src='example.com'></iframe>",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "http://example.com\n",
    expected: {
      urlbar: "http://example.com",
      autocomplete: "http://example.com/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "http://example.com\r",
    expected: {
      urlbar: "http://example.com",
      autocomplete: "http://example.com/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "http://ex\ra\nmp\r\nle.com\r\n",
    expected: {
      urlbar: "http://example.com",
      autocomplete: "http://example.com/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
  {
    input: "127.0.0.1\r",
    expected: {
      urlbar: "127.0.0.1",
      autocomplete: "http://127.0.0.1/",
      type: UrlbarUtils.RESULT_TYPE.URL,
    },
  },
];

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // There are cases that URLBar loses focus before assertion of this test.
      // In that case, this test will be failed since the result is closed
      // before it. We use this pref so that keep the result even if lose focus.
      ["ui.popup.disable_autohide", true],
    ],
  });

  registerCleanupFunction(function() {
    SpecialPowers.clipboardCopyString("");
  });
});

add_task(async function test_paste_onto_urlbar() {
  for (const { input, expected } of TEST_DATA) {
    gURLBar.value = "";
    gURLBar.focus();

    await paste(input);
    await assertResult(expected);

    await UrlbarTestUtils.promisePopupClose(window);
  }
});

add_task(async function test_paste_after_opening_autocomplete_panel() {
  for (const { input, expected } of TEST_DATA) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "",
    });

    await paste(input);
    await assertResult(expected);

    await UrlbarTestUtils.promisePopupClose(window);
  }
});

async function assertResult(expected) {
  Assert.equal(gURLBar.value, expected.urlbar, "Pasted value is correct");

  const result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(
    result.title,
    expected.autocomplete,
    "Title of autocomplete is correct"
  );
  Assert.equal(result.type, expected.type, "Type of autocomplete is correct");
}

async function paste(input) {
  await SimpleTest.promiseClipboardChange(input.replace(/\r\n?/g, "\n"), () => {
    clipboardHelper.copyString(input);
  });

  document.commandDispatcher
    .getControllerForCommand("cmd_paste")
    .doCommand("cmd_paste");
}
