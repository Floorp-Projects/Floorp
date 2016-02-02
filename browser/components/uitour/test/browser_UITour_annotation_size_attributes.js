/*
 * Test that width and height attributes don't get set by widget code on the highlight panel.
 */

"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;
var highlight = document.getElementById("UITourHighlightContainer");
var tooltip = document.getElementById("UITourTooltip");

add_task(setup_UITourTest);

add_UITour_task(function* test_highlight_size_attributes() {
  yield gContentAPI.showHighlight("appMenu");
  yield elementVisiblePromise(highlight,
                              "Highlight should be shown after showHighlight() for the appMenu");
  yield gContentAPI.showHighlight("urlbar");
  yield elementVisiblePromise(highlight, "Highlight should be moved to the urlbar");
  yield new Promise((resolve) => {
    SimpleTest.executeSoon(() => {
      is(highlight.height, "", "Highlight panel should have no explicit height set");
      is(highlight.width, "", "Highlight panel should have no explicit width set");
      resolve();
    });
  });
});

add_UITour_task(function* test_info_size_attributes() {
  yield gContentAPI.showInfo("appMenu", "test title", "test text");
  yield elementVisiblePromise(tooltip, "Tooltip should be shown after showInfo() for the appMenu");
  yield gContentAPI.showInfo("urlbar", "new title", "new text");
  yield elementVisiblePromise(tooltip, "Tooltip should be moved to the urlbar");
  yield new Promise((resolve) => {
    SimpleTest.executeSoon(() => {
      is(tooltip.height, "", "Info panel should have no explicit height set");
      is(tooltip.width, "", "Info panel should have no explicit width set");
      resolve();
    });
  });
});
