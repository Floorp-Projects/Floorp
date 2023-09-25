"use strict";

test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs(
      ["browser.newtabpage.activity-stream.feeds.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.section.topstories", false],
      ["browser.newtabpage.activity-stream.feeds.section.highlights", false]
    );
  },
  test: async function test_render_customizeMenu() {
    function getSection(sectionIdentifier) {
      return content.document.querySelector(
        `section[data-section-id="${sectionIdentifier}"]`
      );
    }
    function promiseSectionShown(sectionIdentifier) {
      return ContentTaskUtils.waitForMutationCondition(
        content.document.querySelector("main"),
        { childList: true, subtree: true },
        () => getSection(sectionIdentifier)
      );
    }
    const TOPSITES_PREF = "browser.newtabpage.activity-stream.feeds.topsites";
    const HIGHLIGHTS_PREF =
      "browser.newtabpage.activity-stream.feeds.section.highlights";
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

    // Test that clicking the shortcuts toggle will make the section
    // appear on the newtab page.
    //
    // We waive XRay wrappers because we want to call the click()
    // method defined on the toggle from this context.
    let shortcutsSwitch = Cu.waiveXrays(
      content.document.querySelector("#shortcuts-section moz-toggle")
    );
    Assert.ok(
      !Services.prefs.getBoolPref(TOPSITES_PREF),
      "Topsites are turned off"
    );
    Assert.ok(!getSection("topsites"), "Shortcuts section is not rendered");

    let sectionShownPromise = promiseSectionShown("topsites");
    shortcutsSwitch.click();
    await sectionShownPromise;

    Assert.ok(getSection("topsites"), "Shortcuts section is rendered");

    // Test that clicking the pocket toggle will make the pocket section
    // appear on the newtab page
    //
    // We waive XRay wrappers because we want to call the click()
    // method defined on the toggle from this context.
    let pocketSwitch = Cu.waiveXrays(
      content.document.querySelector("#pocket-section moz-toggle")
    );
    Assert.ok(
      !Services.prefs.getBoolPref(TOPSTORIES_PREF),
      "Pocket pref is turned off"
    );
    Assert.ok(!getSection("topstories"), "Pocket section is not rendered");

    sectionShownPromise = promiseSectionShown("topstories");
    pocketSwitch.click();
    await sectionShownPromise;

    Assert.ok(getSection("topstories"), "Pocket section is rendered");

    // Test that clicking the recent activity toggle will make the
    // recent activity section appear on the newtab page.
    //
    // We waive XRay wrappers because we want to call the click()
    // method defined on the toggle from this context.
    let highlightsSwitch = Cu.waiveXrays(
      content.document.querySelector("#recent-section moz-toggle")
    );
    Assert.ok(
      !Services.prefs.getBoolPref(HIGHLIGHTS_PREF),
      "Highlights pref is turned off"
    );
    Assert.ok(!getSection("highlights"), "Highlights section is not rendered");

    sectionShownPromise = promiseSectionShown("highlights");
    highlightsSwitch.click();
    await sectionShownPromise;

    Assert.ok(getSection("highlights"), "Highlights section is rendered");
  },
  async after() {
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.topsites"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.section.topstories"
    );
    Services.prefs.clearUserPref(
      "browser.newtabpage.activity-stream.feeds.section.highlights"
    );
  },
});

test_newtab({
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
});
