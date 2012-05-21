/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_FILE = "test-network.html";

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  hud = HUDService.hudReferences[hudId];

  browser.addEventListener("load", tabReload, true);

  content.location.reload();
}

function tabReload(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  let textContent = hud.outputNode.textContent;
  isnot(textContent.indexOf("test-network.html"), -1,
        "found test-network.html");
  isnot(textContent.indexOf("test-image.png"), -1, "found test-image.png");
  isnot(textContent.indexOf("testscript.js"), -1, "found testscript.js");
  isnot(textContent.indexOf("running network console logging tests"), -1,
        "found the console.log() message from testscript.js");

  finishTest();
}

function test() {
  let jar = getJar(getRootDirectory(gTestPath));
  let dir = jar ?
            extractJarToTmp(jar) :
            getChromeDir(getResolvedURI(gTestPath));
  dir.append(TEST_FILE);

  let uri = Services.io.newFileURI(dir);

  addTab(uri.spec);
  browser.addEventListener("load", tabLoad, true);
}
