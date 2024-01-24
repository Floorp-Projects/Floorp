"use strict";

// Test that the customization menu is rendered.
test_newtab({
  test: async function test_render_customizeMenu() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".personalize-button"),
      "Wait for personalize button to load on the newtab page"
    );

    let defaultPos = "matrix(1, 0, 0, 1, 0, 0)";
    Assert.notStrictEqual(
      content.getComputedStyle(
        content.document.querySelector(".customize-menu")
      ).transform,
      defaultPos,
      "Customize Menu should be rendered, but not visible"
    );

    let customizeButton = content.document.querySelector(".personalize-button");
    customizeButton.click();

    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".customize-menu"),
      "Customize Menu should be rendered now"
    );
  },
});
