/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test//browser/test-error.html";

function test() {
  expectUncaughtException();
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testViewSource, false);
}

function testViewSource() {
  browser.removeEventListener("DOMContentLoaded", testViewSource, false);

  openConsole();

  let button = content.document.querySelector("button");
  button = XPCNativeWrapper.unwrap(button);
  ok(button, "we have the button on the page");

  button.addEventListener("click", buttonClicked, false);
  EventUtils.sendMouseEvent({ type: "click" }, button, content);
}

function buttonClicked(aEvent) {
  aEvent.target.removeEventListener("click", buttonClicked, false);
  executeSoon(findLocationNode);
}

let observer = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened") {
      return;
    }

    ok(true, "the view source window was opened in response to clicking " +
       "the location node");

    Services.ww.unregisterNotification(this);

    // executeSoon() is necessary to avoid crashing Firefox. See bug 611543.
    executeSoon(function() {
      aSubject.close();
      finishTest();
    });
  }
};

function findLocationNode() {
  outputNode = HUDService.getHudByWindow(content).outputNode;

  let locationNode = outputNode.querySelector(".webconsole-location");
  ok(locationNode, "we have the location node");

  Services.ww.registerNotification(observer);

  EventUtils.sendMouseEvent({ type: "click" }, locationNode);
}

