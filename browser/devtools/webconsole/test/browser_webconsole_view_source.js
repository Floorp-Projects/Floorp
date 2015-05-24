/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-error.html";

let getItemForAttachment;
let Sources;
let getItemInvoked = false;

add_task(function*() {
  yield loadTab(TEST_URI);
  let hud = yield openConsole(null);
  info("console opened");

  let button = content.document.querySelector("button");
  ok(button, "we have the button on the page");

  expectUncaughtException();
  EventUtils.sendMouseEvent({ type: "click" }, button, content);

  let { panelWin: { DebuggerView } } = yield openDebugger();
  info("debugger opened");
  Sources = DebuggerView.Sources;
  hud = yield openConsole();
  info("console opened again");

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBazBaz is not defined",
      category: CATEGORY_JS,
      severity: SEVERITY_ERROR,
    }],
  });

  let msg = [...result.matched][0];
  ok(msg, "error message");
  let locationNode = msg.querySelector(".message-location");
  ok(locationNode, "location node");

  let onTabOpen = waitForTab();

  getItemForAttachment = Sources.getItemForAttachment;
  Sources.getItemForAttachment = () => {
    getItemInvoked = true;
    return false;
  };

  EventUtils.sendMouseEvent({ type: "click" }, locationNode);

  let tab = yield onTabOpen;
  ok(true, "the view source tab was opened in response to clicking " +
           "the location node");
  gBrowser.removeTab(tab);

  ok(getItemInvoked, "custom getItemForAttachment() was invoked");
  Sources.getItemForAttachment = getItemForAttachment;
  Sources = getItemForAttachment = null;
});
