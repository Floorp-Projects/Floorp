/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The console input should be evaluated with microtask level != 0.

"use strict";

add_task(async function () {
  // Needed for the execute() function below
  await pushPref("security.allow_parent_unrestricted_js_loads", true);

  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  hud.iframeWindow.focus();
  execute(
    hud,
    `
{
  let logCount = 0;

  queueMicrotask(() => {
    console.log("#" + logCount + ": in microtask");
    logCount++;
  });

  console.log("#" + logCount + ": before createXULElement");
  logCount++;

  // This shouldn't perform microtask checkpoint.
  document.createXULElement("browser");

  console.log("#" + logCount + ": after createXULElement");
  logCount++;
}
`
  );

  const beforeCreateXUL = await waitFor(() =>
    findConsoleAPIMessage(hud, "before createXULElement")
  );
  const afterCreateXUL = await waitFor(() =>
    findConsoleAPIMessage(hud, "after createXULElement")
  );
  const inMicroTask = await waitFor(() =>
    findConsoleAPIMessage(hud, "in microtask")
  );

  const getMessageIndex = msg => {
    const text = msg.textContent;
    return text.match(/^#(\d+)/)[1];
  };

  is(
    getMessageIndex(beforeCreateXUL),
    "0",
    "before createXULElement log is displayed first"
  );
  is(
    getMessageIndex(afterCreateXUL),
    "1",
    "after createXULElement log is displayed second"
  );
  is(getMessageIndex(inMicroTask), "2", "in microtask log is displayed last");

  ok(true, "Expected messages are displayed in the browser console");
});
