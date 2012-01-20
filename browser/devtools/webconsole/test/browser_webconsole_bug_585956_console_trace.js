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

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-585956-console-trace.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);

  openConsole();

  browser.addEventListener("load", tabReloaded, true);
  content.location.reload();
}

function tabReloaded() {
  browser.removeEventListener("load", tabReloaded, true);

  // The expected stack trace object.
  let stacktrace = [
    { filename: TEST_URI, lineNumber: 9, functionName: null, language: 2 },
    { filename: TEST_URI, lineNumber: 14, functionName: "foobar585956b", language: 2 },
    { filename: TEST_URI, lineNumber: 18, functionName: "foobar585956a", language: 2 },
    { filename: TEST_URI, lineNumber: 21, functionName: null, language: 2 }
  ];

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];

  let node = HUD.outputNode.querySelector(".hud-log");
  ok(node, "found trace log node");
  ok(node._stacktrace, "found stacktrace object");
  is(node._stacktrace.toSource(), stacktrace.toSource(), "stacktrace is correct");
  isnot(node.textContent.indexOf("bug-585956"), -1, "found file name");

  finishTest();
}
