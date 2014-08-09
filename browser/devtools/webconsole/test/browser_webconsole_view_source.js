/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-error.html";

let getItemForAttachment;
let Sources;
let getItemInvoked = false;

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole(null).then(testViewSource);
  });
}

function testViewSource(hud) {
  info("console opened");

  let button = content.document.querySelector("button");
  ok(button, "we have the button on the page");

  expectUncaughtException();
  EventUtils.sendMouseEvent({ type: "click" }, button, content);

  openDebugger().then(({panelWin: { DebuggerView }}) => {
    info("debugger opened");
    Sources = DebuggerView.Sources;
    openConsole().then((hud) => {
      info("console opened again");

      waitForMessages({
        webconsole: hud,
        messages: [{
          text: "fooBazBaz is not defined",
          category: CATEGORY_JS,
          severity: SEVERITY_ERROR,
        }],
      }).then(onMessage);
    });
  });

  function onMessage([result]) {
    let msg = [...result.matched][0];
    ok(msg, "error message");
    let locationNode = msg.querySelector(".message-location");
    ok(locationNode, "location node");

    Services.ww.registerNotification(observer);

    getItemForAttachment = Sources.getItemForAttachment;
    Sources.getItemForAttachment = () => {
      getItemInvoked = true;
      return false;
    };

    EventUtils.sendMouseEvent({ type: "click" }, locationNode);
  }
}

let observer = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "domwindowopened") {
      return;
    }

    ok(true, "the view source window was opened in response to clicking " +
       "the location node");

    aSubject.close();
    ok(getItemInvoked, "custom getItemForAttachment() was invoked");
    Sources.getItemForAttachment = getItemForAttachment;
    Sources = getItemForAttachment = null;
    finishTest();
  }
};

registerCleanupFunction(function() {
  Services.ww.unregisterNotification(observer);
});
