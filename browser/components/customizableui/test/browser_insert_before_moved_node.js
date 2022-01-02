"use strict";

/**
 * Check inserting before a node that has moved from the toolbar into a
 * non-customizable bit of the browser works.
 */
add_task(async function() {
  for (let toolbar of ["nav-bar", "TabsToolbar"]) {
    CustomizableUI.createWidget({
      id: "real-button",
      label: "test real button",
    });
    CustomizableUI.addWidgetToArea("real-button", toolbar);
    CustomizableUI.addWidgetToArea("moved-button-not-here", toolbar);
    let placements = CustomizableUI.getWidgetIdsInArea(toolbar);
    Assert.deepEqual(
      placements.slice(-2),
      ["real-button", "moved-button-not-here"],
      "Should have correct placements"
    );
    let otherButton = document.createXULElement("toolbarbutton");
    otherButton.id = "moved-button-not-here";
    if (toolbar == "nav-bar") {
      gURLBar.textbox.parentNode.appendChild(otherButton);
    } else {
      gBrowser.tabContainer.appendChild(otherButton);
    }
    CustomizableUI.destroyWidget("real-button");
    CustomizableUI.createWidget({
      id: "real-button",
      label: "test real button",
    });

    let button = document.getElementById("real-button");
    ok(button, "Button should exist");
    if (button) {
      let expectedContainer = CustomizableUI.getCustomizationTarget(
        document.getElementById(toolbar)
      );
      is(
        button.parentNode,
        expectedContainer,
        "Button should be in the toolbar"
      );
    }

    CustomizableUI.destroyWidget("real-button");
    otherButton.remove();
    CustomizableUI.reset();
  }
});
