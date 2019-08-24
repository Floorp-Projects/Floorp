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
    "#computed-container .computed-property-view"
  )) {
    const nameSpan = property.querySelector(".computed-property-name");
    const valueSpan = property.querySelector(".computed-property-value");

    if (nameSpan.firstChild.textContent === name) {
      prop = { nameSpan: nameSpan, valueSpan: valueSpan };
      break;
    }
  }
  return prop;
}

// Test that styles for elements can be viewed when using web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_inspector_styles.html", {
    waitForRecording: true,
    disableLogging: true,
  });
  const { threadFront } = dbg;

  await threadFront.interrupt();
  await threadFront.resume();

  await threadFront.interrupt();

  const { inspector, view } = await openComputedView();
  await checkBackgroundColor("body", "rgb(0, 128, 0)");
  await checkBackgroundColor("#maindiv", "rgb(0, 0, 255)");

  const bp = await setBreakpoint(threadFront, "doc_inspector_styles.html", 11);

  await rewindToLine(threadFront, 11);
  await checkBackgroundColor("#maindiv", "rgb(255, 0, 0)");

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);

  async function checkBackgroundColor(node, color) {
    await selectNode(node, inspector);

    const value = getComputedViewProperty(view, "background-color").valueSpan;
    const nodeInfo = view.getNodeInfo(value);
    is(nodeInfo.value.property, "background-color");
    is(nodeInfo.value.value, color);
  }
});
