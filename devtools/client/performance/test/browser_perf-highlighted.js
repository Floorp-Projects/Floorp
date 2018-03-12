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

add_task(function* () {
  let { target, toolbox, console } = yield initConsoleInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let tab = toolbox.doc.getElementById("toolbox-tab-performance");

  yield console.profile("rust");
  yield waitUntil(() => tab.classList.contains("highlighted"));

  ok(tab.classList.contains("highlighted"), "Performance tab is highlighted during " +
    "recording from console.profile when unloaded.");

  yield console.profileEnd("rust");
  yield waitUntil(() => !tab.classList.contains("highlighted"));

  ok(!tab.classList.contains("highlighted"),
    "Performance tab is no longer highlighted when console.profile recording finishes.");

  let { panel } = yield initPerformanceInTab({ tab: target.tab });

  yield startRecording(panel);

  ok(tab.classList.contains("highlighted"),
    "Performance tab is highlighted during recording while in performance tool.");

  yield stopRecording(panel);

  ok(!tab.classList.contains("highlighted"),
    "Performance tab is no longer highlighted when recording finishes.");

  yield teardownToolboxAndRemoveTab(panel);
});
