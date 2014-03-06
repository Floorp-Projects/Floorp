/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that width and height attributes don't get set by widget code on the highlight panel.
 */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;
let highlight = document.getElementById("UITourHighlightContainer");
let tooltip = document.getElementById("UITourTooltip");

Components.utils.import("resource:///modules/UITour.jsm");

function test() {
  UITourTest();
}

let tests = [
  function test_highlight_size_attributes(done) {
    gContentAPI.showHighlight("appMenu");
    waitForElementToBeVisible(highlight, function moveTheHighlight() {
      gContentAPI.showHighlight("urlbar");
      waitForElementToBeVisible(highlight, function checkPanelAttributes() {
        SimpleTest.executeSoon(() => {
          ise(highlight.height, "", "Highlight panel should have no explicit height set");
          ise(highlight.width, "", "Highlight panel should have no explicit width set");
          done();
        });
      }, "Highlight should be moved to the urlbar");
    }, "Highlight should be shown after showHighlight() for the appMenu");
  },

  function test_info_size_attributes(done) {
    gContentAPI.showInfo("appMenu", "test title", "test text");
    waitForElementToBeVisible(tooltip, function moveTheTooltip() {
      gContentAPI.showInfo("urlbar", "new title", "new text");
      waitForElementToBeVisible(tooltip, function checkPanelAttributes() {
        SimpleTest.executeSoon(() => {
          ise(tooltip.height, "", "Info panel should have no explicit height set");
          ise(tooltip.width, "", "Info panel should have no explicit width set");
          done();
        });
      }, "Tooltip should be moved to the urlbar");
    }, "Tooltip should be shown after showInfo() for the appMenu");
  },

];
