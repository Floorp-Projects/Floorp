/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the toolbox tab for performance is highlighted when recording,
 * whether already loaded, or via console.profile with an unloaded performance tools.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInTab, initConsoleInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { waitUntil } = require("devtools/client/performance/test/helpers/wait-utils");

add_task(async function() {
  const { target, toolbox, console } = await initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  const tab = toolbox.doc.getElementById("toolbox-tab-performance");

  await console.profile("rust");
  await waitUntil(() => tab.classList.contains("highlighted"));

  ok(tab.classList.contains("highlighted"), "Performance tab is highlighted during " +
    "recording from console.profile when unloaded.");

  await console.profileEnd("rust");
  await waitUntil(() => !tab.classList.contains("highlighted"));

  ok(!tab.classList.contains("highlighted"),
    "Performance tab is no longer highlighted when console.profile recording finishes.");

  const { panel } = await initPerformanceInTab({ tab: target.tab });

  await startRecording(panel);

  ok(tab.classList.contains("highlighted"),
    "Performance tab is highlighted during recording while in performance tool.");

  await stopRecording(panel);

  ok(!tab.classList.contains("highlighted"),
    "Performance tab is no longer highlighted when recording finishes.");

  await teardownToolboxAndRemoveTab(panel);
});
