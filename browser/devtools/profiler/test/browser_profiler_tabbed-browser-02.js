/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler displays and organizes the recording data in tabs,
 * spawning a new tab from frame nodes when required.
 */

const WAIT_TIME = 1000; // ms

let gPrevShowPlatformData =
  Services.prefs.getBoolPref("devtools.profiler.ui.show-platform-data");

let test = Task.async(function*() {
  let [target, debuggee, panel] = yield initFrontend(SIMPLE_URL);
  let { $, $$, EVENTS, Prefs, ProfileView } = panel.panelWin;

  // Make sure platform data is displayed, so there's samples available.
  Prefs.showPlatformData = true;

  yield startRecording(panel);
  busyWait(WAIT_TIME); // allow the profiler module to sample more cpu activity
  yield stopRecording(panel, { waitForDisplay: true });

  let treeRoot = ProfileView._getCallTreeRoot();
  ok(treeRoot,
    "There's a call tree view available on the profile view.");
  let callItem = treeRoot.getChild(0);
  ok(callItem,
    "The first displayed item in the tree was retreived.");
  let callNode = callItem.target;
  ok(callNode,
    "The first displayed item in the tree has a corresponding DOM node.");

  is($(".call-tree-name", callNode).getAttribute("value"), "Startup::XRE_Main",
    "The first displayed item in the tree has the expected function name.");
  is($(".call-tree-cell[type=percentage]", callNode).getAttribute("value"), "100%",
    "The first displayed item in the tree has the expected percentage usage.");

  let tabSpawned = panel.panelWin.once(EVENTS.TAB_SPAWNED_FROM_FRAME_NODE);
  EventUtils.sendMouseEvent({ type: "mousedown" }, callNode.querySelector(".call-tree-zoom"));
  yield tabSpawned;

  is(ProfileView.tabCount, 2,
    "There should be two tabs visible now.");
  is($("#profile-pane").selectedPanel, $("#profile-content"),
    "The profile content should still be displayed in the profile view.");
  is($("#profile-content").selectedIndex, 1,
    "The second tab is now selected.");

  let firstTabTitle = $$("#profile-content .tab-title-label")[0].getAttribute("value");
  let secondTabTitle = $$("#profile-content .tab-title-label")[1].getAttribute("value");
  info("The first tab's title is: " + firstTabTitle);
  info("The second tab's title is: " + secondTabTitle);

  isnot(firstTabTitle, secondTabTitle,
    "The first and second tab titles are different.");
  ok(firstTabTitle.match(/\d+ ms . \d+ ms/),
    "The recording's first tab title is correct.");
  ok(secondTabTitle.match(/[\d\.,]+ ms . [\d\.,]+ ms/),
    "The recording's second tab title is correct.");

  let newTreeRoot = ProfileView._getCallTreeRoot();
  ok(newTreeRoot,
    "There's a call tree view available on the profile view again.");
  let newCallItem = newTreeRoot.getChild(0);
  ok(newCallItem,
    "The first displayed item in the tree was retreived again.");
  let newCallNode = newCallItem.target;
  ok(newCallNode,
    "The first displayed item in the tree has a corresponding DOM node again.");

  isnot(treeRoot, newTreeRoot,
    "The new call tree view has a different root this time.");
  isnot(callNode, newCallNode,
    "The new call tree view has a different corresponding DOM node this time.");

  is($(".call-tree-name", newTreeRoot.target).getAttribute("value"), "Startup::XRE_Main",
    "The first displayed item in the tree has the expected function name.");
  is($(".call-tree-cell[type=percentage]", newTreeRoot.target).getAttribute("value"), "100%",
    "The first displayed item in the tree has the expected percentage usage.");

  yield teardown(panel);
  finish();
});

registerCleanupFunction(() => {
  Services.prefs.setBoolPref(
    "devtools.profiler.ui.show-platform-data", gPrevShowPlatformData);
});
