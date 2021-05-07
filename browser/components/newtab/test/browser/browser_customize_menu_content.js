"use strict";

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs(
      ["browser.newtabpage.activity-stream.newNewtabExperience.enabled", true],
      ["browser.newtabpage.activity-stream.customizationMenu.enabled", true],
      ["browser.newtabpage.activity-stream.feeds.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.section.topstories", false],
      ["browser.newtabpage.activity-stream.feeds.section.highlights", false],
      ["browser.newtabpage.activity-stream.feeds.snippets", false]
    );
  },
  test: async function test_render_customizeMenu() {
    const TOPSITES_PREF = "browser.newtabpage.activity-stream.feeds.topsites";
    const HIGHLIGHTS_PREF =
      "browser.newtabpage.activity-stream.feeds.section.highlights";
    const SNIPPETS_PREF = "browser.newtabpage.activity-stream.feeds.snippets";
    const TOPSTORIES_PREF =
      "browser.newtabpage.activity-stream.feeds.section.topstories";

    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".personalize-button"),
      "Wait for prefs button to load on the newtab page"
    );

    let customizeButton = content.document.querySelector(".personalize-button");
    customizeButton.click();

    let defaultPos = "matrix(1, 0, 0, 1, 0, 0)";
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".customize-menu")
        ).transform === defaultPos,
      "Customize Menu should be visible on screen"
    );

    // Test that clicking the shortcuts toggle will make the section appear on the newtab page.
    let shortcutsSwitch = content.document.querySelector(
      "#shortcuts-section .switch"
    );
    let shortcutsSection = content.document.querySelector(
      "section[data-section-id='topsites']"
    );
    Assert.ok(
      !Services.prefs.getBoolPref(TOPSITES_PREF),
      "Topsites are turned off"
    );
    Assert.ok(!shortcutsSection, "Shortcuts section is not rendered");

    let prefPromise = ContentTaskUtils.waitForCondition(
      () => Services.prefs.getBoolPref(TOPSITES_PREF),
      "TopSites pref is turned on"
    );
    shortcutsSwitch.click();
    await prefPromise;

    Assert.ok(
      content.document.querySelector("section[data-section-id='topsites']"),
      "Shortcuts section is rendered"
    );

    // Test that clicking the pocket toggle will make the pocket section appear on the newtab page
    let pocketSwitch = content.document.querySelector(
      "#pocket-section .switch"
    );
    Assert.ok(
      !Services.prefs.getBoolPref(TOPSTORIES_PREF),
      "Pocket pref is turned off"
    );
    Assert.ok(
      !content.document.querySelector("section[data-section-id='topstories']"),
      "Pocket section is not rendered"
    );

    prefPromise = ContentTaskUtils.waitForCondition(
      () => Services.prefs.getBoolPref(TOPSTORIES_PREF),
      "Pocket pref is turned on"
    );
    pocketSwitch.click();
    await prefPromise;

    Assert.ok(
      content.document.querySelector("section[data-section-id='topstories']"),
      "Pocket section is rendered"
    );

    // Test that clicking the recent activity toggle will make the recent activity section appear on the newtab page
    let highlightsSwitch = content.document.querySelector(
      "#recent-section .switch"
    );
    Assert.ok(
      !Services.prefs.getBoolPref(HIGHLIGHTS_PREF),
      "Highlights pref is turned off"
    );
    Assert.ok(
      !content.document.querySelector("section[data-section-id='highlights']"),
      "Highlights section is not rendered"
    );

    prefPromise = ContentTaskUtils.waitForCondition(
      () => Services.prefs.getBoolPref(HIGHLIGHTS_PREF),
      "Highlights pref is turned on"
    );
    highlightsSwitch.click();
    await prefPromise;

    Assert.ok(
      content.document.querySelector("section[data-section-id='highlights']"),
      "Highlights section is rendered"
    );

    // Test that clicking the snippets toggle will flip the snippets pref
    // note: Snippets are disabled in tests.
    let snippetsSwitch = content.document.querySelector(
      "#snippets-section .switch"
    );
    Assert.ok(
      !Services.prefs.getBoolPref(SNIPPETS_PREF),
      "Snippets pref is turned off"
    );

    prefPromise = ContentTaskUtils.waitForCondition(
      () => Services.prefs.getBoolPref(SNIPPETS_PREF),
      "Snippets pref is turned on after click"
    );
    snippetsSwitch.click();
    await prefPromise;

    prefPromise = ContentTaskUtils.waitForCondition(
      () => !Services.prefs.getBoolPref(SNIPPETS_PREF),
      "Snippets pref is turned on after click"
    );
    snippetsSwitch.click();
    await prefPromise;
  },
  async after() {
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.customizationMenu.enabled"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.topsites"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.section.topstories"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.section.highlights"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.snippets"
    );
  },
});

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      true,
    ]);
  },
  test: async function test_open_close_customizeMenu() {
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".personalize-button"),
      "Wait for prefs button to load on the newtab page"
    );

    let customizeButton = content.document.querySelector(".personalize-button");
    customizeButton.click();

    let defaultPos = "matrix(1, 0, 0, 1, 0, 0)";
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".customize-menu")
        ).transform === defaultPos,
      "Customize Menu should be visible on screen"
    );

    await ContentTaskUtils.waitForCondition(
      () => content.document.activeElement.classList.contains("close-button"),
      "Close button should be focused when menu becomes visible"
    );

    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".personalize-button")
        ).visibility === "hidden",
      "Personalize button should become hidden"
    );

    // Test close button.
    let closeButton = content.document.querySelector(".close-button");
    closeButton.click();
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".customize-menu")
        ).transform !== defaultPos,
      "Customize Menu should not be visible anymore"
    );

    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.activeElement.classList.contains("personalize-button"),
      "Personalize button should be focused when menu closes"
    );

    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".personalize-button")
        ).visibility === "visible",
      "Personalize button should become visible"
    );

    // Reopen the customize menu
    customizeButton.click();
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".customize-menu")
        ).transform === defaultPos,
      "Customize Menu should be visible on screen now"
    );

    // Test closing with esc key.
    EventUtils.synthesizeKey("VK_ESCAPE", {}, content);
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".customize-menu")
        ).transform !== defaultPos,
      "Customize Menu should not be visible anymore"
    );

    // Reopen the customize menu
    customizeButton.click();
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".customize-menu")
        ).transform === defaultPos,
      "Customize Menu should be visible on screen now"
    );

    // Test closing with external click.
    let outerWrapper = content.document.querySelector(".outer-wrapper");
    outerWrapper.click();
    await ContentTaskUtils.waitForCondition(
      () =>
        content.getComputedStyle(
          content.document.querySelector(".customize-menu")
        ).transform !== defaultPos,
      "Customize Menu should not be visible anymore"
    );
  },
  async after() {
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled"
    );
  },
});
