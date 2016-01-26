/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that long strings can be expanded in the console output.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,test for bug 787981 - check " +
                 "that long strings can be expanded in the output.";

add_task(function* () {
  let { DebuggerServer } = require("devtools/server/main");

  let longString = (new Array(DebuggerServer.LONG_STRING_LENGTH + 4))
                    .join("a") + "foobar";
  let initialString =
    longString.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH);

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  hud.jsterm.clearOutput(true);
  hud.jsterm.execute("console.log('bazbaz', '" + longString +"', 'boom')");

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.log output",
      text: ["bazbaz", "boom", initialString],
      noText: "foobar",
      longString: true,
    }],
  });

  let clickable = result.longStrings[0];
  ok(clickable, "long string ellipsis is shown");

  clickable.scrollIntoView(false);

  EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "full string",
      text: ["bazbaz", "boom", longString],
      category: CATEGORY_WEBDEV,
      longString: false,
    }],
  });

  hud.jsterm.clearOutput(true);
  let msg = yield execute(hud, "'" + longString + "'");

  isnot(msg.textContent.indexOf(initialString), -1,
      "initial string is shown");
  is(msg.textContent.indexOf(longString), -1,
      "full string is not shown");

  clickable = msg.querySelector(".longStringEllipsis");
  ok(clickable, "long string ellipsis is shown");

  clickable.scrollIntoView(false);

  EventUtils.synthesizeMouse(clickable, 3, 4, {}, hud.iframeWindow);

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "full string",
      text: longString,
      category: CATEGORY_OUTPUT,
      longString: false,
    }],
  });
});

function execute(hud, str) {
  let deferred = promise.defer();
  hud.jsterm.execute(str, deferred.resolve);
  return deferred.promise;
}
