/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-error.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testViewSource);
  }, true);
}

function testViewSource(hud) {
  let button = content.document.querySelector("button");
  ok(button, "we have the button on the page");

  expectUncaughtException();
  EventUtils.sendMouseEvent({ type: "click" }, button, content);

  waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBazBaz is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  }).then(([result]) => {
    Cu.forceGC();
    let msg = [...result.matched][0];
    ok(msg, "error message");
    let locationNode = msg.querySelector(".location");
    ok(locationNode, "location node");

    Services.ww.registerNotification(observer);

    EventUtils.sendMouseEvent({ type: "click" }, locationNode);
  });
}

let observer = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened") {
      return;
    }

    ok(true, "the view source window was opened in response to clicking " +
       "the location node");

    // executeSoon() is necessary to avoid crashing Firefox. See bug 611543.
    executeSoon(function() {
      aSubject.close();
      finishTest();
    });
  }
};

registerCleanupFunction(function() {
  Services.ww.unregisterNotification(observer);
});
