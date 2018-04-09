/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that console API calls in the content page appear in the browser console.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>console API calls`;

add_task(async function() {
  await addTab(TEST_URI);
  const hud = await HUDService.toggleBrowserConsole();

  const contentArgs = {
    log: "MyLog",
    warn: "MyWarn",
    error: "MyError",
    info: "MyInfo",
    debug: "MyDebug",
    counterName: "MyCounter",
    timerName: "MyTimer",
  };

  const expectedMessages = [
    contentArgs.log,
    contentArgs.warn,
    contentArgs.error,
    contentArgs.info,
    contentArgs.debug,
    `${contentArgs.counterName}: 1`,
    `${contentArgs.timerName}:`,
    `console.trace`,
    `Assertion failed`,
  ];
  const onAllMessages = Promise.all(expectedMessages.map(m => waitForMessage(hud, m)));

  ContentTask.spawn(gBrowser.selectedBrowser, contentArgs, function(args) {
    content.console.log(args.log);
    content.console.warn(args.warn);
    content.console.error(args.error);
    content.console.info(args.info);
    content.console.debug(args.debug);
    content.console.count(args.counterName);
    content.console.time(args.timerName);
    content.console.timeEnd(args.timerName);
    content.console.trace();
    content.console.assert(false, "err");
  });

  await onAllMessages;

  ok(true, "Expected messages are displayed in the browser console");
});
