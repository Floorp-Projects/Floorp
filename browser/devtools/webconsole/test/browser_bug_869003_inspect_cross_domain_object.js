/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that users can inspect objects logged from cross-domain iframes -
// bug 869003.

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-bug-869003-top-window.html";

var test = asyncTest(function* () {
  // This test is slightly more involved: it opens the web console, then the
  // variables view for a given object, it updates a property in the view and
  // checks the result. We can get a timeout with debug builds on slower
  // machines.
  requestLongerTimeout(2);

  yield loadTab("data:text/html;charset=utf8,<p>hello");
  let hud = yield openConsole();

  content.location = TEST_URI;

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      name: "console.log message",
      text: "foobar",
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      objects: true,
    }],
  });

  let msg = [...result.matched][0];
  ok(msg, "message element");

  let body = msg.querySelector(".message-body");
  ok(body, "message body");

  let clickable = result.clickableElements[0];
  ok(clickable, "clickable object found");
  ok(body.textContent.includes('{ hello: "world!",'), "message text check");

  executeSoon(() => {
    EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);
  });

  let aVar = yield hud.jsterm.once("variablesview-fetched");
  ok(aVar, "variables view fetched");
  ok(aVar._variablesView, "variables view object");

  [result] = yield findVariableViewProperties(aVar, [
    { name: "hello", value: "world!" },
    { name: "bug", value: 869003 },
  ], { webconsole: hud });

  let prop = result.matchedProp;
  ok(prop, "matched the |hello| property in the variables view");

  // Check that property value updates work.
  aVar = yield updateVariablesViewProperty({
    property: prop,
    field: "value",
    string: "'omgtest'",
    webconsole: hud,
  });

  info("onFetchAfterUpdate");

  yield findVariableViewProperties(aVar, [
    { name: "hello", value: "omgtest" },
    { name: "bug", value: 869003 },
  ], { webconsole: hud });
});

