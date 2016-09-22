/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that the variables view sidebar can be closed by pressing Escape in the
// web console.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<script>let fooObj = {testProp: 'testValue'}</script>";

add_task(function* () {
  let hud = yield openNewTabAndConsole(TEST_URI);
  let jsterm = hud.jsterm;
  let vview;

  yield openSidebar("fooObj", 'testProp: "testValue"');
  vview.window.focus();

  let sidebarClosed = jsterm.once("sidebar-closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield sidebarClosed;

  function* openSidebar(objName, expectedText) {
    yield jsterm.execute(objName);
    info("JSTerm executed");

    let msg = yield waitFor(() => findMessage(hud, "Object"));
    ok(msg, "Message found");

    let anchor = msg.querySelector("a");
    let body = msg.querySelector(".message-body");
    ok(anchor, "object anchor");
    ok(body, "message body");
    ok(body.textContent.includes(expectedText), "message text check");

    msg.scrollIntoView();
    yield EventUtils.synthesizeMouse(anchor, 2, 2, {}, hud.iframeWindow);

    let vviewVar = yield jsterm.once("variablesview-fetched");
    vview = vviewVar._variablesView;
    ok(vview, "variables view object exists");
  }
});
