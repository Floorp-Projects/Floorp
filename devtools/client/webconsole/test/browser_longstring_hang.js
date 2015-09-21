/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that very long strings do not hang the browser.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-859170-longstring-hang.html";

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  info("wait for the initial long string");

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "find 'foobar', no 'foobaz', in long string output",
        text: "foobar",
        noText: "foobaz",
        category: CATEGORY_WEBDEV,
        longString: true,
      },
    ],
  });

  let clickable = results[0].longStrings[0];
  ok(clickable, "long string ellipsis is shown");
  clickable.scrollIntoView(false);

  EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);

  info("wait for long string expansion");

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "find 'foobaz' after expand, but no 'boom!' at the end",
        text: "foobaz",
        noText: "boom!",
        category: CATEGORY_WEBDEV,
        longString: false,
      },
      {
        text: "too long to be displayed",
        longString: false,
      },
    ],
  });
});
