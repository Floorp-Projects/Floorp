/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests to ensure that errors don't appear when the console is closed while a
// completion is being performed. See Bug 580001.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function() {
  const tab = await addTab(TEST_URI);
  const browser = tab.linkedBrowser;
  const hud = await openConsole();

  // Fire a completion.
  await setInputValueForAutocompletion(hud, "doc");

  let errorWhileClosing = false;
  function errorListener() {
    errorWhileClosing = true;
  }

  browser.addEventListener("error", errorListener);
  const onToolboxDestroyed = gDevTools.once("toolbox-destroyed");

  // Focus the jsterm and perform the keycombo to close the WebConsole.
  hud.jsterm.focus();
  EventUtils.synthesizeKey("i", {
    accelKey: true,
    [Services.appinfo.OS == "Darwin" ? "altKey" : "shiftKey"]: true,
  });

  await onToolboxDestroyed;

  browser.removeEventListener("error", errorListener);
  is(errorWhileClosing, false, "no error while closing the WebConsole");
});
