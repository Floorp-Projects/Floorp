/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests to ensure that errors don't appear when the console is closed while a
// completion is being performed.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

add_task(function* () {
  let { browser } = yield loadTab(TEST_URI);

  let hud = yield openConsole();
  yield testClosingAfterCompletion(hud, browser);
});

function testClosingAfterCompletion(hud, browser) {
  let deferred = promise.defer();

  let errorWhileClosing = false;
  function errorListener() {
    errorWhileClosing = true;
  }

  browser.addEventListener("error", errorListener, false);

  // Focus the jsterm and perform the keycombo to close the WebConsole.
  hud.jsterm.focus();

  gDevTools.once("toolbox-destroyed", function () {
    browser.removeEventListener("error", errorListener, false);
    is(errorWhileClosing, false, "no error while closing the WebConsole");
    deferred.resolve();
  });

  if (Services.appinfo.OS == "Darwin") {
    EventUtils.synthesizeKey("i", { accelKey: true, altKey: true });
  } else {
    EventUtils.synthesizeKey("i", { accelKey: true, shiftKey: true });
  }

  return deferred.promise;
}
