/* exported getTargetForSelectedTab, waitFor, plotFPS */
"use strict";
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  TabTargetFactory,
} = require("devtools/client/framework/tab-target-factory");
const Services = require("Services");

// Always log packets when running tests.
Services.prefs.setBoolPref("devtools.debugger.log", true);
SimpleTest.registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.debugger.log");
});

SimpleTest.waitForExplicitFinish();

/**
 * Add a new test tab in the browser and load the given url.
 * @return Promise a promise that resolves to the new target representing
 *         the page currently opened.
 */
function getTargetForSelectedTab() {
  // Get the target and get the necessary front
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  return TabTargetFactory.forTab(gBrowser.selectedTab);
}

function waitFor(time) {
  return new Promise(resolve => setTimeout(resolve, time));
}

function plotFPS(ticks, interval = 100, clamp = 60) {
  const timeline = [];
  const totalTicks = ticks.length;

  // If the refresh driver didn't get a chance to tick before the
  // recording was stopped, assume framerate was 0.
  if (totalTicks == 0) {
    timeline.push({ delta: 0, value: 0 });
    timeline.push({ delta: interval, value: 0 });
    return timeline;
  }

  let frameCount = 0;
  let prevTime = ticks[0];

  for (let i = 1; i < totalTicks; i++) {
    const currTime = ticks[i];
    frameCount++;

    const elapsedTime = currTime - prevTime;
    if (elapsedTime < interval) {
      continue;
    }

    const framerate = Math.min(1000 / (elapsedTime / frameCount), clamp);
    timeline.push({ delta: prevTime, value: framerate });
    timeline.push({ delta: currTime, value: framerate });

    frameCount = 0;
    prevTime = currTime;
  }

  return timeline;
}
