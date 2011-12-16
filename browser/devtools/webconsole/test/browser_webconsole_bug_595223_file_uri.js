/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Web Console test suite.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
