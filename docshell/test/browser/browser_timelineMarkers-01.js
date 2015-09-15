/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the docShell has the right profile timeline API

var test = Task.async(function*() {
  waitForExplicitFinish();

  yield openUrl("data:text/html;charset=utf-8,Test page");

  let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);

  ok("recordProfileTimelineMarkers" in docShell,
    "The recordProfileTimelineMarkers attribute exists");
  ok("popProfileTimelineMarkers" in docShell,
    "The popProfileTimelineMarkers function exists");
  ok(docShell.recordProfileTimelineMarkers === false,
    "recordProfileTimelineMarkers is false by default");
  ok(docShell.popProfileTimelineMarkers().length === 0,
    "There are no markers by default");

  docShell.recordProfileTimelineMarkers = true;
  ok(docShell.recordProfileTimelineMarkers === true,
    "recordProfileTimelineMarkers can be set to true");

  docShell.recordProfileTimelineMarkers = false;
  ok(docShell.recordProfileTimelineMarkers === false,
    "recordProfileTimelineMarkers can be set to false");

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
