/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

function getComputedViewProperty(view, name) {
  let prop;
  for (const property of view.styleDocument.querySelectorAll(
      "#computed-container .computed-property-view")) {
    const nameSpan = property.querySelector(".computed-property-name");
    const valueSpan = property.querySelector(".computed-property-value");

    if (nameSpan.firstChild.textContent === name) {
      prop = {nameSpan: nameSpan, valueSpan: valueSpan};
      break;
    }
  }
  return prop;
}

// Test that styles for elements can be viewed when using web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger(
    "doc_inspector_styles.html",
    { waitForRecording: true }
  );
  const {threadClient, tab, toolbox} = dbg;
  await threadClient.resume();

  await threadClient.interrupt();

  const {inspector, view} = await openComputedView();
  await checkBackgroundColor("body", "rgb(0, 128, 0)");
  await checkBackgroundColor("#maindiv", "rgb(0, 0, 255)");

  const bp = await setBreakpoint(threadClient, "doc_inspector_styles.html", 11);

  await rewindToLine(threadClient, 11);
  await checkBackgroundColor("#maindiv", "rgb(255, 0, 0)");

  await threadClient.removeBreakpoint(bp);
  await toolbox.closeToolbox();
  await gBrowser.removeTab(tab);

  async function checkBackgroundColor(node, color) {
    await selectNode(node, inspector);

    const value = getComputedViewProperty(view, "background-color").valueSpan;
    const nodeInfo = view.getNodeInfo(value);
    is(nodeInfo.value.property, "background-color");
    is(nodeInfo.value.value, color);
  }
});
