/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that objects given to console.log() are inspectable.

"use strict";

add_task(function* () {
  yield loadTab("data:text/html;charset=utf8,test for bug 676722 - " +
                "inspectable objects for window.console");

  let hud = yield openConsole();
  hud.jsterm.clearOutput(true);

  yield hud.jsterm.execute("myObj = {abba: 'omgBug676722'}");
  hud.jsterm.execute("console.log('fooBug676722', myObj)");

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "fooBug676722",
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
  ok(clickable, "the console.log() object anchor was found");
  ok(body.textContent.includes('{ abba: "omgBug676722" }'),
     "clickable node content is correct");

  executeSoon(() => {
    EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);
  });

  let varView = yield hud.jsterm.once("variablesview-fetched");
  ok(varView, "object inspector opened on click");

  yield findVariableViewProperties(varView, [{
    name: "abba",
    value: "omgBug676722",
  }], { webconsole: hud });
});
