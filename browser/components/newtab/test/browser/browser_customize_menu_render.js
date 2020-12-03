"use strict";

// Test that the customization menu is rendered when
// the parent pref is set to true even if the customize
// menu specific pref is false.
test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      true,
    ]);
    await pushPrefs([
      "browser.newtabpage.activity-stream.customizationMenu.enabled",
      false,
    ]);
  },
  test: async function test_render_customizeMenu() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".personalize-button"),
      "Wait for prefs button to load on the newtab page"
    );

    let customizeMenu = content.document.querySelector(".customize-menu");
    ok(!customizeMenu, "Customize Menu should not be rendered yet");

    let customizeButton = content.document.querySelector(".personalize-button");
    customizeButton.click();

    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".customize-menu"),
      "Customize Menu should be rendered now"
    );
  },
  async after() {
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.customizationMenu.enabled"
    );
  },
});

// Test that the customization menu is rendered when the parent
// pref is set to false and the customize menu specific pref is
// set to true.
test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      false,
    ]);
    await pushPrefs([
      "browser.newtabpage.activity-stream.customizationMenu.enabled",
      true,
    ]);
  },
  test: async function test_render_customizeMenu() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".personalize-button"),
      "Wait for prefs button to load on the newtab page"
    );

    let customizeMenu = content.document.querySelector(".customize-menu");
    ok(!customizeMenu, "Customize Menu should not be rendered yet");

    let customizeButton = content.document.querySelector(".personalize-button");
    customizeButton.click();

    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".customize-menu"),
      "Customize Menu should be rendered now"
    );
  },
  async after() {
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.customizationMenu.enabled"
    );
  },
});

// Test that the customization menu is rendered when both the parent pref and
// the customize menu specific pref is set to true.
test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      true,
    ]);
    await pushPrefs([
      "browser.newtabpage.activity-stream.customizationMenu.enabled",
      true,
    ]);
  },
  test: async function test_render_customizeMenu() {
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".personalize-button"),
      "Wait for prefs button to load on the newtab page"
    );

    let customizeMenu = content.document.querySelector(".customize-menu");
    ok(!customizeMenu, "Customize Menu should not be rendered yet");

    let customizeButton = content.document.querySelector(".personalize-button");
    customizeButton.click();

    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".customize-menu"),
      "Customize Menu should be rendered now"
    );
  },
  async after() {
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.customizationMenu.enabled"
    );
  },
});
