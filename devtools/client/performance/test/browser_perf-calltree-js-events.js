/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the call tree up/down events work for js calltrees.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { synthesizeProfile } = require("devtools/client/performance/test/helpers/synth-utils");
const { once } = require("devtools/client/performance/test/helpers/event-utils");
const { ThreadNode } = require("devtools/client/performance/modules/logic/tree-model");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const { EVENTS, $, DetailsView, OverviewView, JsCallTreeView } = panel.panelWin;

  await startRecording(panel);
  await stopRecording(panel);

  const rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  await DetailsView.selectView("js-calltree");
  await rendered;

  // Mock the profile used so we can get a deterministic tree created.
  const profile = synthesizeProfile();
  const threadNode = new ThreadNode(profile.threads[0], OverviewView.getTimeInterval());
  JsCallTreeView._populateCallTree(threadNode);
  JsCallTreeView.emit(EVENTS.UI_JS_CALL_TREE_RENDERED);

  const firstTreeItem = $("#js-calltree-view .call-tree-item");

  // DE-XUL: There are focus issues with XUL. Focus first, then synthesize the clicks
  // so that keyboard events work correctly.
  firstTreeItem.focus();

  let count = 0;
  const onFocus = () => count++;
  JsCallTreeView.on("focus", onFocus);

  click(firstTreeItem);

  key("VK_DOWN");
  key("VK_DOWN");
  key("VK_DOWN");
  key("VK_DOWN");

  JsCallTreeView.off("focus", onFocus);
  is(count, 4, "Several focus events are fired for the calltree.");

  await teardownToolboxAndRemoveTab(panel);
});
