/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure iframe scripts don't disappear after few reloads (bug 1259743)
 */

"use strict";

const IFRAME_URL = "data:text/html;charset=utf-8," +
  "<script>function fn() { console.log('hello'); }</script>" +
  "<div onclick='fn()'>hello</div>";
const TAB_URL = `data:text/html;charset=utf-8,<iframe src="${IFRAME_URL}"/>`;

add_task(async function() {
  let [,, panel] = await initDebugger();
  let dbg = panel.panelWin;
  let newSource;

  newSource = waitForDebuggerEvents(panel, dbg.EVENTS.NEW_SOURCE);
  reload(panel, TAB_URL);
  await newSource;
  ok(true, "Source event fired on initial load");

  for (let i = 0; i < 5; i++) {
    newSource = waitForDebuggerEvents(panel, dbg.EVENTS.NEW_SOURCE);
    reload(panel);
    await newSource;
    ok(true, `Source event fired after ${i + 1} reloads`);
  }

  await closeDebuggerAndFinish(panel);
});
