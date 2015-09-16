/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF = "devtools.webconsole.persistlog";
const TEST_FILE = "test-network.html";
const TEST_URI = "data:text/html;charset=utf8,<p>test file URI";

var hud;

var test = asyncTest(function* () {
  Services.prefs.setBoolPref(PREF, true);

  let jar = getJar(getRootDirectory(gTestPath));
  let dir = jar ?
            extractJarToTmp(jar) :
            getChromeDir(getResolvedURI(gTestPath));

  dir.append(TEST_FILE);
  let uri = Services.io.newFileURI(dir);

  let { browser } = yield loadTab(TEST_URI);

  hud = yield openConsole();
  hud.jsterm.clearOutput();

  let loaded = loadBrowser(browser);
  content.location = uri.spec;
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
