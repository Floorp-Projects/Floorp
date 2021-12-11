/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// XXX Remove this when the file is migrated to the new frontend.
/* eslint-disable no-undef */

// See Bug 595223.

const PREF = "devtools.webconsole.persistlog";
const TEST_FILE = "test-network.html";

var hud;

add_task(async function() {
  Services.prefs.setBoolPref(PREF, true);

  const jar = getJar(getRootDirectory(gTestPath));
  const dir = jar
    ? extractJarToTmp(jar)
    : getChromeDir(getResolvedURI(gTestPath));

  dir.append(TEST_FILE);
  const uri = Services.io.newFileURI(dir);

  // Open tab with correct remote type so we don't switch processes when we load
  // the file:// URI, otherwise we won't get the same web console.
  const remoteType = E10SUtils.getRemoteTypeForURI(
    uri.spec,
    gMultiProcessBrowser,
    gFissionBrowser
  );
  await loadTab("about:blank", remoteType);

  hud = await openConsole();
  await clearOutput(hud);

  await navigateTo(uri.spec);

  await testMessages();

  Services.prefs.clearUserPref(PREF);
  hud = null;
});

function testMessages() {
  return waitForMessages({
    webconsole: hud,
    messages: [
      {
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
      },
    ],
  });
}
