/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the Marker Details view renders all properties expected
 * for each marker.
 */

function* spawnTest() {
  let { target, panel } = yield initPerformance(MARKERS_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, WaterfallView } = panel.panelWin;
  let { L10N } = devtools.require("devtools/performance/global");

  // Hijack the markers massaging part of creating the waterfall view,
  // to prevent collapsing markers and allowing this test to verify
  // everything individually. A better solution would be to just expand
  // all markers first and then skip the meta nodes, but I'm lazy.
  WaterfallView._prepareWaterfallTree = markers => {
    return { submarkers: markers };
  };

  const MARKER_TYPES = [
    "Styles", "Reflow", "Paint", "ConsoleTime", "TimeStamp"
  ];

  yield startRecording(panel);
  yield waitUntil(() => {
    // Wait until we get 3 different markers.
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    return MARKER_TYPES.every(type => markers.some(m => m.name === type));
  });
  yield stopRecording(panel);

  // Select everything
  let timeline = OverviewView.graphs.get("timeline");
  let rerendered = WaterfallView.once(EVENTS.WATERFALL_RENDERED);
  timeline.setSelection({ start: 0, end: timeline.width });
  yield rerendered;

  let bars = $$(".waterfall-marker-bar");
  let markers = PerformanceController.getCurrentRecording().getMarkers();

  ok(bars.length >= MARKER_TYPES.length, `Got at least ${MARKER_TYPES.length} markers (1)`);
  ok(markers.length >= MARKER_TYPES.length, `Got at least ${MARKER_TYPES.length} markers (2)`);

  const tests = {
    ConsoleTime: function (marker) {
      shouldHaveLabel($, L10N.getStr("timeline.markerDetail.consoleTimerName"), "!!!", marker);
      shouldHaveStack($, "startStack", marker);
      shouldHaveStack($, "endStack", marker);
      return true;
    },
    TimeStamp: function (marker) {
      shouldHaveLabel($, "Label:", "go", marker);
      shouldHaveStack($, "stack", marker);
      return true;
    },
    Styles: function (marker) {
      if (marker.restyleHint) {
        shouldHaveLabel($, "Restyle Hint:", marker.restyleHint.replace(/eRestyle_/g, ""), marker);
      }
      if (marker.stack) {
        shouldHaveStack($, "stack", marker);
        return true;
      }
    },
    Reflow: function (marker) {
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
  let TOTAL_TESTS = 4;

  for (let i = 0; i < bars.length; i++) {
    let bar = bars[i];
    let m = markers[i];
    EventUtils.sendMouseEvent({ type: "mousedown" }, bar);

    if (testsDone.indexOf(m.name) === -1 && tests[m.name]) {
      let fullTestComplete = tests[m.name](m);
      if (fullTestComplete) {
        testsDone.push(m.name);
      }
    } else {
      info(`TODO: Need to add marker details tests for ${m.name}`);
    }

    if (testsDone.length === TOTAL_TESTS) {
      break;
    }
  }

  yield teardown(panel);
  finish();
}

function shouldHaveStack ($, type, marker) {
  ok($(`#waterfall-details .marker-details-stack[type=${type}]`), `${marker.name} has a stack: ${type}`);
}

function shouldHaveLabel ($, name, value, marker) {
  info(name);
  let $name = $(`#waterfall-details .marker-details-labelcontainer .marker-details-labelname[value="${name}"]`);
  let $value = $name.parentNode.querySelector(".marker-details-labelvalue");
  is($value.getAttribute("value"), value, `${marker.name} has correct label for ${name}:${value}`);
}
