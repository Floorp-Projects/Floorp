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
 * The Original Code is WebConsole test for bug 618078.
 *
 * The Initial Developer of the Original Code is
 * Mihai Sucan.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

// Tests that network log messages bring up the network panel.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-618078-network-exceptions.html";

let testEnded = false;

let TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aSubject)
  {
    if (testEnded || !(aSubject instanceof Ci.nsIScriptError)) {
      return;
    }

    is(aSubject.category, "content javascript", "error category");

    if (aSubject.category == "content javascript") {
      executeSoon(checkOutput);
    }
    else {
      testEnd();
    }
  }
};

function checkOutput()
{
  if (testEnded) {
    return;
  }

  let textContent = hud.outputNode.textContent;
  isnot(textContent.indexOf("bug618078exception"), -1,
        "exception message");

  testEnd();
}

function testEnd()
{
  if (testEnded) {
    return;
  }

  testEnded = true;
  Services.console.unregisterListener(TestObserver);
  finishTest();
}

function test()
{
  addTab("data:text/html;charset=utf-8,Web Console test for bug 618078");

  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    openConsole();

    let hudId = HUDService.getHudIdByWindow(content);
    hud = HUDService.hudReferences[hudId];

    Services.console.registerListener(TestObserver);
    registerCleanupFunction(testEnd);

    executeSoon(function() {
      expectUncaughtException();
      content.location = TEST_URI;
    });
  }, true);
}

