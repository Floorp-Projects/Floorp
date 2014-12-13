/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the docShell profile timeline API returns the right markers when
// restyles, reflows and paints occur

let URL = '<!DOCTYPE html><style>' +
          'body {margin:0; padding: 0;} ' +
          'div {width:100px;height:100px;background:red;} ' +
          '.resize-change-color {width:50px;height:50px;background:blue;} ' +
          '.change-color {width:50px;height:50px;background:yellow;} ' +
          '.add-class {}' +
          '</style><div></div>';

let TESTS = [{
  desc: "Changing the width of the test element",
  setup: function(div) {
    div.setAttribute("class", "resize-change-color");
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    console.log(markers);
    info(JSON.stringify(markers.filter(m => m.name == "Paint")));
    ok(markers.some(m => m.name == "Reflow"), "markers includes Reflow");
    ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
    for (let marker of markers.filter(m => m.name == "Paint")) {
      // This change should generate at least one rectangle.
      ok(marker.rectangles.length >= 1, "marker has one rectangle");
      // One of the rectangles should contain the div.
      ok(marker.rectangles.some(r => rectangleContains(r, 0, 0, 100, 100)));
    }
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "Changing the test element's background color",
  setup: function(div) {
    div.setAttribute("class", "change-color");
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    ok(!markers.some(m => m.name == "Reflow"), "markers doesn't include Reflow");
    ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
    for (let marker of markers.filter(m => m.name == "Paint")) {
      // This change should generate at least one rectangle.
      ok(marker.rectangles.length >= 1, "marker has one rectangle");
      // One of the rectangles should contain the div.
      ok(marker.rectangles.some(r => rectangleContains(r, 0, 0, 50, 50)));
    }
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "Changing the test element's classname",
  setup: function(div) {
    div.setAttribute("class", "change-color add-class");
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    ok(!markers.some(m => m.name == "Reflow"), "markers doesn't include Reflow");
    ok(!markers.some(m => m.name == "Paint"), "markers doesn't include Paint");
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "sync console.time/timeEnd",
  setup: function(div, docShell) {
    content.console.time("FOOBAR");
    content.console.timeEnd("FOOBAR");
    let markers = docShell.popProfileTimelineMarkers();
    is(markers.length, 1, "Got one marker");
    is(markers[0].name, "ConsoleTime", "Got ConsoleTime marker");
    is(markers[0].causeName, "FOOBAR", "Got ConsoleTime FOOBAR detail");
    content.console.time("FOO");
    content.setTimeout(() => {
      content.console.time("BAR");
      content.setTimeout(() => {
        content.console.timeEnd("FOO");
        content.console.timeEnd("BAR");
      }, 100);
    }, 100);
  },
  check: function(markers) {
    is(markers.length, 2, "Got 2 markers");
    is(markers[0].name, "ConsoleTime", "Got first ConsoleTime marker");
    is(markers[0].causeName, "FOO", "Got ConsoleTime FOO detail");
    is(markers[1].name, "ConsoleTime", "Got second ConsoleTime marker");
    is(markers[1].causeName, "BAR", "Got ConsoleTime BAR detail");
  }
}];

let test = Task.async(function*() {
  waitForExplicitFinish();

  yield openUrl("data:text/html;charset=utf8," + encodeURIComponent(URL));

  let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);

  let div = content.document.querySelector("div");

  info("Start recording");
  docShell.recordProfileTimelineMarkers = true;

  for (let {desc, setup, check} of TESTS) {

    info("Running test: " + desc);

    info("Flushing the previous markers if any");
    docShell.popProfileTimelineMarkers();

    info("Running the test setup function");
    let onMarkers = waitForMarkers(docShell);
    setup(div, docShell);
    info("Waiting for new markers on the docShell");
    let markers = yield onMarkers;

    info("Running the test check function");
    check(markers);
  }

  info("Stop recording");
  docShell.recordProfileTimelineMarkers = false;

  gBrowser.removeCurrentTab();
  finish();
});

function openUrl(url) {
  return new Promise(function(resolve, reject) {
    window.focus();

    let tab = window.gBrowser.selectedTab = window.gBrowser.addTab(url);
    let linkedBrowser = tab.linkedBrowser;

    linkedBrowser.addEventListener("load", function onload() {
      linkedBrowser.removeEventListener("load", onload, true);
      resolve(tab);
    }, true);
  });
}

function waitForMarkers(docshell) {
  return new Promise(function(resolve, reject) {
    let waitIterationCount = 0;
    let maxWaitIterationCount = 10; // Wait for 2sec maximum

    let interval = setInterval(() => {
      let markers = docshell.popProfileTimelineMarkers();
      if (markers.length > 0) {
        clearInterval(interval);
        resolve(markers);
      }
      if (waitIterationCount > maxWaitIterationCount) {
        clearInterval(interval);
        resolve([]);
      }
      waitIterationCount++;
    }, 200);
  });
}

function rectangleContains(rect, x, y, width, height) {
  return rect.x <= x && rect.y <= y && rect.width >= width &&
    rect.height >= height;
}
