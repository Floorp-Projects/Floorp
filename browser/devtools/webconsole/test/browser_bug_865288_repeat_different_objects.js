/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that makes sure messages are not considered repeated when console.log()
// is invoked with different objects, see bug 865288.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-repeated-messages.html";

let hud = null;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(aHud) {
  hud = aHud;

  // Check that css warnings are not coalesced if they come from different lines.
  info("waiting for 3 console.log objects");

  hud.jsterm.clearOutput(true);
  content.wrappedJSObject.testConsoleObjects();

  waitForMessages({
    webconsole: hud,
    messages: [{
      name: "3 console.log messages",
      text: "abba",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      count: 3,
      repeats: 1,
      objects: true,
    }],
  }).then(checkMessages);
}

function checkMessages([result])
{
  let msgs = [...result.matched];
  is(msgs.length, 3, "3 message elements");
  let m = -1;

  function nextMessage()
  {
    let msg = msgs[++m];
    if (msg) {
      ok(msg, "message element #" + m);

      let clickable = msg.querySelector(".body a");
      ok(clickable, "clickable object #" + m);

      msg.scrollIntoView(false);
      clickObject(clickable);
    }
    else {
      finishTest();
    }
  }

  nextMessage();

  function clickObject(aObject)
  {
    hud.jsterm.once("variablesview-fetched", onObjectFetch);
    EventUtils.synthesizeMouse(aObject, 2, 2, {}, hud.iframeWindow);
  }

  function onObjectFetch(aEvent, aVar)
  {
    findVariableViewProperties(aVar, [
      { name: "id", value: "abba" + m },
    ], { webconsole: hud }).then(nextMessage);
  }
}
