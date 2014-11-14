/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view correctly shows the searchbox
 * when prompted.
 */

const TAB_URL = EXAMPLE_URL + "doc_with-frame.html";

let gTab, gPanel, gDebugger;
let gVariables;

function test() {
  initDebugger(TAB_URL).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    waitForSourceShown(gPanel, ".html").then(performTest);
  });
}

function performTest() {
  // Step 1: the searchbox shouldn't initially be shown.

  ok(!gVariables._searchboxNode,
    "There should not initially be a searchbox available in the variables view.");
  ok(!gVariables._searchboxContainer,
    "There should not initially be a searchbox container available in the variables view.");
  ok(!gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should not be found.");

  // Step 2: test enable/disable cycles.

  gVariables._enableSearch();
  ok(gVariables._searchboxNode,
    "There should be a searchbox available after enabling.");
  ok(gVariables._searchboxContainer,
    "There should be a searchbox container available after enabling.");
  ok(gVariables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should be found.");

  gVariables._disableSearch();
  ok(!gVariables._searchboxNode,
    "There shouldn't be a searchbox available after disabling.");
  ok(!gVariables._searchboxContainer,
    "There shouldn't be a searchbox container available after disabling.");
  ok(!gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should not be found.");

  // Step 3: add a placeholder while the searchbox is hidden.

  var placeholder = "not freshly squeezed mango juice";

  gVariables.searchPlaceholder = placeholder;
  is(gVariables.searchPlaceholder, placeholder,
    "The placeholder getter didn't return the expected string");

  // Step 4: enable search and check the placeholder.

  gVariables._enableSearch();
  ok(gVariables._searchboxNode,
    "There should be a searchbox available after enabling.");
  ok(gVariables._searchboxContainer,
    "There should be a searchbox container available after enabling.");
  ok(gVariables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should be found.");

  is(gVariables._searchboxNode.getAttribute("placeholder"),
    placeholder, "There correct placeholder should be applied to the searchbox.");

  // Step 5: add a placeholder while the searchbox is visible and check wether
  // it has been immediatey applied.

  var placeholder = "freshly squeezed mango juice";

  gVariables.searchPlaceholder = placeholder;
  is(gVariables.searchPlaceholder, placeholder,
    "The placeholder getter didn't return the expected string");

  is(gVariables._searchboxNode.getAttribute("placeholder"),
    placeholder, "There correct placeholder should be applied to the searchbox.");

  // Step 4: disable, enable, then test the placeholder.

  gVariables._disableSearch();
  ok(!gVariables._searchboxNode,
    "There shouldn't be a searchbox available after disabling again.");
  ok(!gVariables._searchboxContainer,
    "There shouldn't be a searchbox container available after disabling again.");
  ok(!gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should not be found.");

  gVariables._enableSearch();
  ok(gVariables._searchboxNode,
    "There should be a searchbox available after enabling again.");
  ok(gVariables._searchboxContainer,
    "There should be a searchbox container available after enabling again.");
  ok(gVariables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should be found.");

  is(gVariables._searchboxNode.getAttribute("placeholder"),
    placeholder, "There correct placeholder should be applied to the searchbox again.");

  // Step 5: alternate disable, enable, then test the placeholder.

  gVariables.searchEnabled = false;
  ok(!gVariables._searchboxNode,
    "There shouldn't be a searchbox available after disabling again.");
  ok(!gVariables._searchboxContainer,
    "There shouldn't be a searchbox container available after disabling again.");
  ok(!gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should not be found.");

  gVariables.searchEnabled = true;
  ok(gVariables._searchboxNode,
    "There should be a searchbox available after enabling again.");
  ok(gVariables._searchboxContainer,
    "There should be a searchbox container available after enabling again.");
  ok(gVariables._searchboxContainer.hidden,
    "The searchbox container should be hidden at this point.");
  ok(gVariables._parent.parentNode.querySelector(".variables-view-searchinput"),
    "The searchbox element should be found.");

  is(gVariables._searchboxNode.getAttribute("placeholder"),
    placeholder, "There correct placeholder should be applied to the searchbox again.");

  closeDebuggerAndFinish(gPanel);
}

registerCleanupFunction(function() {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
