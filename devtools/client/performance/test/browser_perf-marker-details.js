/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the Marker Details view renders all properties expected
 * for each marker.
 */

function* spawnTest() {
  let { target, panel } = yield initPerformance(MARKERS_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, WaterfallView } = panel.panelWin;

  // Hijack the markers massaging part of creating the waterfall view,
  // to prevent collapsing markers and allowing this test to verify
  // everything individually. A better solution would be to just expand
  // all markers first and then skip the meta nodes, but I'm lazy.
  WaterfallView._prepareWaterfallTree = markers => {
    return { submarkers: markers };
  };

  const MARKER_TYPES = [
    "Styles", "Reflow", "ConsoleTime", "TimeStamp"
  ];

  yield startRecording(panel);
  ok(true, "Recording has started.");

  yield waitUntil(() => {
    // Wait until we get all the different markers.
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    return MARKER_TYPES.every(type => markers.some(m => m.name === type));
  });

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  info("No need to select everything in the timeline.");
  info("All the markers should be displayed by default.");

  let bars = Array.prototype.filter.call($$(".waterfall-marker-bar"),
             (bar) => MARKER_TYPES.indexOf(bar.getAttribute("type")) !== -1);
  let markers = PerformanceController.getCurrentRecording().getMarkers()
                .filter(m => MARKER_TYPES.indexOf(m.name) !== -1);

  info(`Got ${bars.length} bars and ${markers.length} markers.`);
  info("Markers types from datasrc: " + Array.map(markers, e => e.name));
  info("Markers names from sidebar: " + Array.map(bars, e => e.parentNode.parentNode.querySelector(".waterfall-marker-name").getAttribute("value")));

  ok(bars.length >= MARKER_TYPES.length, `Got at least ${MARKER_TYPES.length} markers (1)`);
  ok(markers.length >= MARKER_TYPES.length, `Got at least ${MARKER_TYPES.length} markers (2)`);

  // Sanity check that markers are in chronologically ascending order
  markers.reduce((previous, m) => {
    if (m.start <= previous) {
      ok(false, "Markers are not in order");
      info(markers);
    }
    return m.start;
  }, 0);

  // Override the timestamp marker's stack with our own recursive stack, which
  // can happen for unknown reasons (bug 1246555); we should not cause a crash
  // when attempting to render a recursive stack trace
  let timestampMarker = markers.find(m => m.name === "ConsoleTime");
  ok(typeof timestampMarker.stack === "number", "ConsoleTime marker has a stack before overwriting.");
  let frames = PerformanceController.getCurrentRecording().getFrames();
  let frameIndex = timestampMarker.stack = frames.length;
  frames.push({ line: 1, column: 1, source: "file.js", functionDisplayName: "test", parent: frameIndex + 1});
  frames.push({ line: 1, column: 1, source: "file.js", functionDisplayName: "test", parent: frameIndex + 2 });
  frames.push({ line: 1, column: 1, source: "file.js", functionDisplayName: "test", parent: frameIndex });

  const tests = {
    ConsoleTime: function (marker) {
      info("Got `ConsoleTime` marker with data: " + JSON.stringify(marker));
      ok(marker.stack === frameIndex, "Should have the ConsoleTime marker with recursive stack");
      shouldHaveStack($, "startStack", marker);
      shouldHaveStack($, "endStack", marker);
      shouldHaveLabel($, "Timer Name:", "!!!", marker);
      return true;
    },
    TimeStamp: function (marker) {
      info("Got `TimeStamp` marker with data: " + JSON.stringify(marker));
      shouldHaveLabel($, "Label:", "go", marker);
      shouldHaveStack($, "stack", marker);
      return true;
    },
    Styles: function (marker) {
      info("Got `Styles` marker with data: " + JSON.stringify(marker));
      if (marker.stack) {
        shouldHaveStack($, "stack", marker);
        return true;
      }
    },
    Reflow: function (marker) {
      info("Got `Reflow` marker with data: " + JSON.stringify(marker));
      if (marker.stack) {
        shouldHaveStack($, "stack", marker);
        return true;
      }
    }
  };

  // Keep track of all marker tests that are finished so we only
  // run through each marker test once, so we don't spam 500 redundant
  // tests.
  let testsDone = [];

  for (let i = 0; i < bars.length; i++) {
    let bar = bars[i];
    let m = markers[i];
    EventUtils.sendMouseEvent({ type: "mousedown" }, bar);

    if (tests[m.name]) {
      if (testsDone.indexOf(m.name) === -1) {
        let fullTestComplete = tests[m.name](m);
        if (fullTestComplete) {
          testsDone.push(m.name);
        }
      }
    } else {
      throw new Error(`No tests for ${m.name} -- should be filtered out.`);
    }

    if (testsDone.length === Object.keys(tests).length) {
      break;
    }
  }

  yield teardown(panel);
  finish();
}

function shouldHaveStack($, type, marker) {
  ok($(`#waterfall-details .marker-details-stack[type=${type}]`), `${marker.name} has a stack: ${type}`);
}

function shouldHaveLabel($, name, value, marker) {
  let $name = $(`#waterfall-details .marker-details-labelcontainer .marker-details-labelname[value="${name}"]`);
  let $value = $name.parentNode.querySelector(".marker-details-labelvalue");
  is($value.getAttribute("value"), value, `${marker.name} has correct label for ${name}:${value}`);
}
/* eslint-enable */
