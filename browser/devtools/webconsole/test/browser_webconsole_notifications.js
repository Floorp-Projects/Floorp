/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for notifications";

function test() {
  observer.init();
  addTab(TEST_URI);
  browser.addEventListener("load", onLoad, true);
}

function webConsoleCreated(aID)
{
  Services.obs.removeObserver(observer, "web-console-created");
  ok(HUDService.getHudReferenceById(aID), "We have a hud reference");
  content.wrappedJSObject.console.log("adding a log message");
}

function webConsoleDestroyed(aID)
{
  Services.obs.removeObserver(observer, "web-console-destroyed");
  ok(!HUDService.getHudReferenceById(aID), "We do not have a hud reference");
  executeSoon(finishTest);
}

function webConsoleMessage(aID, aNodeID)
{
  Services.obs.removeObserver(observer, "web-console-message-created");
  ok(aID, "we have a console ID");
  is(typeof aNodeID, "string", "message node id is a string");
  executeSoon(closeConsole);
}

let observer = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function observe(aSubject, aTopic, aData)
  {
    aSubject = aSubject.QueryInterface(Ci.nsISupportsString);

    switch(aTopic) {
      case "web-console-created":
        webConsoleCreated(aSubject.data);
        break;
      case "web-console-destroyed":
        webConsoleDestroyed(aSubject.data);
        break;
      case "web-console-message-created":
        webConsoleMessage(aSubject, aData);
        break;
      default:
        break;
    }
  },

  init: function init()
  {
    Services.obs.addObserver(this, "web-console-created", false);
    Services.obs.addObserver(this, "web-console-destroyed", false);
    Services.obs.addObserver(this, "web-console-message-created", false);
  }
};

function onLoad() {
  browser.removeEventListener("load", onLoad, true);
  openConsole();
}
