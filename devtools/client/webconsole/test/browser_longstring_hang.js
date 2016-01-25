/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that very long strings do not hang the browser.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-859170-longstring-hang.html";

add_task(function* () {
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
