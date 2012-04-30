/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-603750-websocket.html";
const pref_ws = "network.websocket.enabled";
const pref_block = "network.websocket.override-security-block";

let errors = 0;
let lastWindowId = 0;
let oldPref_ws;

let TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aSubject)
  {
    if (!(aSubject instanceof Ci.nsIScriptError)) {
      return;
    }

    is(aSubject.category, "Web Socket", "received a Web Socket error");
    isnot(aSubject.sourceName.indexOf("test-bug-603750-websocket.js"), -1,
          "sourceName is correct");

    if (++errors == 2) {
      executeSoon(performTest);
    }
    else {
      lastWindowId = aSubject.outerWindowID;
    }
  }
};

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  hud = HUDService.hudReferences[hudId];

  Services.console.registerListener(TestObserver);

  content.location = TEST_URI;
}

function performTest() {
  let textContent = hud.outputNode.textContent;
  isnot(textContent.indexOf("ws://0.0.0.0:81"), -1,
        "first error message found");
  isnot(textContent.indexOf("ws://0.0.0.0:82"), -1,
        "second error message found");

  Services.console.unregisterListener(TestObserver);
  Services.prefs.setBoolPref(pref_ws, oldPref_ws);
  finishTest();
}

function test() {
  oldPref_ws = Services.prefs.getBoolPref(pref_ws);

  Services.prefs.setBoolPref(pref_ws, true);

  addTab("data:text/html;charset=utf-8,Web Console test for bug 603750: Web Socket errors");
  browser.addEventListener("load", tabLoad, true);
}

