/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF = "devtools.webconsole.persistlog";
const TEST_FILE = "test-network.html";

var hud;

add_task(function* () {
  Services.prefs.setBoolPref(PREF, true);

  let jar = getJar(getRootDirectory(gTestPath));
  let dir = jar ?
            extractJarToTmp(jar) :
            getChromeDir(getResolvedURI(gTestPath));

  dir.append(TEST_FILE);
  let uri = Services.io.newFileURI(dir);

  // Open tab with correct remote type so we don't switch processes when we load
  // the file:// URI, otherwise we won't get the same web console.
  let remoteType = E10SUtils.getRemoteTypeForURI(uri.spec,
                                                 gMultiProcessBrowser);
  let { browser } = yield loadTab("about:blank", remoteType);

  hud = yield openConsole();
  hud.jsterm.clearOutput();

  let loaded = loadBrowser(browser);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, uri.spec);
  yield loaded;

  yield testMessages();

  Services.prefs.clearUserPref(PREF);
  hud = null;
});

function testMessages() {
  return waitForMessages({
    webconsole: hud,
    messages: [{
      text: "running network console logging tests",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
    },
    {
      text: "test-network.html",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    },
    {
      text: "test-image.png",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    },
    {
      text: "testscript.js",
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  });
}
