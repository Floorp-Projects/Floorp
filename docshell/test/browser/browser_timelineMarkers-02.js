/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the docShell profile timeline API returns the right markers when
// restyles, reflows and paints occur

let URL = 'data:text/html;charset=utf-8,<!DOCTYPE html><style>div.x{}</style>' +
          '<div style="width:20px;height:2px;background:red"></div>';

let TESTS = [{
  desc: "Changing the width of the test element",
  setup: function(div) {
    div.style.width = "10px";
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    ok(markers.some(m => m.name == "Reflow"), "markers includes Reflow");
    ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "Changing the test element's background color",
  setup: function(div) {
    div.style.backgroundColor = "green";
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    ok(!markers.some(m => m.name == "Reflow"), "markers doesn't include Reflow");
    ok(markers.some(m => m.name == "Paint"), "markers includes Paint");
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}, {
  desc: "Changing the test element's classname",
  setup: function(div) {
    div.className = "x";
  },
  check: function(markers) {
    ok(markers.length > 0, "markers were returned");
    ok(!markers.some(m => m.name == "Reflow"), "markers doesn't include Reflow");
    ok(!markers.some(m => m.name == "Paint"), "markers doesn't include Paint");
    ok(markers.some(m => m.name == "Styles"), "markers includes Restyle");
  }
}];

let test = Task.async(function*() {
  waitForExplicitFinish();

  yield openUrl(URL);

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
    setup(div);

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
    gBrowser.selectedTab = gBrowser.addTab(url);
    gBrowser.selectedBrowser.addEventListener("load", function onload() {
      gBrowser.selectedBrowser.removeEventListener("load", onload, true);
      waitForFocus(resolve, content);
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
