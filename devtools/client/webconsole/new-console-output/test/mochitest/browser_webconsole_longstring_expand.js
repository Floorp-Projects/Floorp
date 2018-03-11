/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from head.js */

// Test that long strings can be expanded in the console output.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,test for bug 787981 - check " +
                 "that long strings can be expanded in the output.";

add_task(async function () {
  let { DebuggerServer } = require("devtools/server/main");

  let longString = (new Array(DebuggerServer.LONG_STRING_LENGTH + 4))
                    .join("a") + "foobar";
  let initialString =
    longString.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH);

  await loadTab(TEST_URI);

  let hud = await openConsole();

  hud.jsterm.clearOutput(true);
  hud.jsterm.execute("console.log('bazbaz', '" + longString + "', 'boom')");

  let [result] = await waitForMessages({
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

  await waitForMessages({
    webconsole: hud,
    messages: [{
      name: "full string",
      text: ["bazbaz", "boom", longString],
      category: CATEGORY_WEBDEV,
      longString: false,
    }],
  });

  hud.jsterm.clearOutput(true);
  let msg = await execute(hud, "'" + longString + "'");

  isnot(msg.textContent.indexOf(initialString), -1,
      "initial string is shown");
  is(msg.textContent.indexOf(longString), -1,
      "full string is not shown");

  clickable = msg.querySelector(".longStringEllipsis");
  ok(clickable, "long string ellipsis is shown");

  clickable.scrollIntoView(false);

  EventUtils.synthesizeMouse(clickable, 3, 4, {}, hud.iframeWindow);

  await waitForMessages({
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
  let deferred = defer();
  hud.jsterm.execute(str, deferred.resolve);
  return deferred.promise;
}
