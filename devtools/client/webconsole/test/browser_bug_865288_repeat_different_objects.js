/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test that makes sure messages are not considered repeated when console.log()
// is invoked with different objects, see bug 865288.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-repeated-messages.html";

var test = asyncTest(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  info("waiting for 3 console.log objects");

  hud.jsterm.clearOutput(true);
  hud.jsterm.execute("window.testConsoleObjects()");

  let [result] = yield waitForMessages({
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
  });

  let msgs = [...result.matched];
  is(msgs.length, 3, "3 message elements");

  for (let i = 0; i < msgs.length; i++) {
    info("test message element #" + i);

    let msg = msgs[i];
    let clickable = msg.querySelector(".message-body a");
    ok(clickable, "clickable object #" + i);

    msg.scrollIntoView(false);
    yield clickObject(clickable, i);
  }

  function* clickObject(obj, i) {
    executeSoon(() => {
      EventUtils.synthesizeMouse(obj, 2, 2, {}, hud.iframeWindow);
    });

    let varView = yield hud.jsterm.once("variablesview-fetched");
    ok(varView, "variables view fetched #" + i);

    yield findVariableViewProperties(varView, [
      { name: "id", value: "abba" + i },
    ], { webconsole: hud });
  }
});

