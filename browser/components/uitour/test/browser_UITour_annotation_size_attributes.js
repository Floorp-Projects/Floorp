/*
 * Test that width and height attributes don't get set by widget code on the highlight panel.
 */

"use strict";

var gTestTab;
var gContentAPI;
var highlight = UITour.getHighlightContainerAndMaybeCreate(document);
var tooltip = UITour.getTooltipAndMaybeCreate(document);

add_task(setup_UITourTest);

add_UITour_task(async function test_highlight_size_attributes() {
  await gContentAPI.showHighlight("appMenu");
  await elementVisiblePromise(
    highlight,
    "Highlight should be shown after showHighlight() for the appMenu"
  );
  await gContentAPI.showHighlight("urlbar");
  await elementVisiblePromise(
    highlight,
    "Highlight should be moved to the urlbar"
  );
  await new Promise(resolve => {
    SimpleTest.executeSoon(() => {
      is(
        highlight.height,
        "",
        "Highlight panel should have no explicit height set"
      );
      is(
        highlight.width,
        "",
        "Highlight panel should have no explicit width set"
      );
      resolve();
    });
  });
});

add_UITour_task(async function test_info_size_attributes() {
  await gContentAPI.showInfo("appMenu", "test title", "test text");
  await elementVisiblePromise(
    tooltip,
    "Tooltip should be shown after showInfo() for the appMenu"
  );
  await gContentAPI.showInfo("urlbar", "new title", "new text");
  await elementVisiblePromise(tooltip, "Tooltip should be moved to the urlbar");
  await new Promise(resolve => {
    SimpleTest.executeSoon(() => {
      is(tooltip.height, "", "Info panel should have no explicit height set");
      is(tooltip.width, "", "Info panel should have no explicit width set");
      resolve();
    });
  });
});
