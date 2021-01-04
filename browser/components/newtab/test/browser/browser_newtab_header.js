"use strict";

// If the pref `browser.newtabpage.activity-stream.newNewtabExperience.enabled`
// is set to true, test that:
// 1. Top sites header is hidden and the topsites section is not collapsed on load".
// 2. Pocket header and section are visible and not collapsed on load.
// 3. Recent activity section and header are visible and not collapsed on load.
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
    // Top sites section
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".top-sites"),
      "Wait for the top sites section to load"
    );

    let topSitesHeader = content.document.querySelector(
      ".top-sites .section-title"
    );
    ok(
      topSitesHeader && topSitesHeader.style.visibility === "hidden",
      "Top sites header should not be visible"
    );

    let topSitesSection = content.document.querySelector(".top-sites");
    let isTopSitesCollapsed = topSitesSection.className.includes("collapsed");
    ok(!isTopSitesCollapsed, "Top sites should not be collapsed on load");

    // Pocket section
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("section[data-section-id='topstories']"),
      "Wait for the pocket section to load"
    );

    let pocketSection = content.document.querySelector(
      "section[data-section-id='topstories']"
    );
    let isPocketSectionCollapsed = pocketSection.className.includes(
      "collapsed"
    );
    ok(
      !isPocketSectionCollapsed,
      "Pocket section should not be collapsed on load"
    );

    let pocketHeader = content.document.querySelector(
      "section[data-section-id='topstories'] .section-title"
    );
    ok(
      pocketHeader && !pocketHeader.style.visibility,
      "Pocket header should be visible"
    );

    // Highlights (Recent activity) section.
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("section[data-section-id='highlights']"),
      "Wait for the highlights section to load"
    );
    let highlightsSection = content.document.querySelector(
      "section[data-section-id='topstories']"
    );
    let isHighlightsSectionCollapsed = highlightsSection.className.includes(
      "collapsed"
    );
    ok(
      !isHighlightsSectionCollapsed,
      "Highlights section should not be collapsed on load"
    );

    let highlightsHeader = content.document.querySelector(
      "section[data-section-id='highlights'] .section-title"
    );
    ok(
      highlightsHeader && !highlightsHeader.style.visibility,
      "Highlights header should be visible"
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

// If the pref `browser.newtabpage.activity-stream.customizationMenu.enabled`
// is set to true, test that:
// 1. Top sites header is hidden and the section is not collapsed on load.
// 2. Pocket header and section are visible.
// 3. Recent activity section and header are visible.
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
    // Top sites section
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".top-sites"),
      "Wait for the top sites section to load"
    );

    let topSitesHeader = content.document.querySelector(
      ".top-sites .section-title"
    );
    ok(
      topSitesHeader && topSitesHeader.style.visibility === "hidden",
      "Top sites header should not be visible"
    );

    let topSitesSection = content.document.querySelector(".top-sites");
    let isTopSitesCollapsed = topSitesSection.className.includes("collapsed");
    ok(!isTopSitesCollapsed, "Top sites should not be collapsed on load");

    // Pocket section
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("section[data-section-id='topstories']"),
      "Wait for the pocket section to load"
    );

    let pocketHeader = content.document.querySelector(
      "section[data-section-id='topstories'] .section-title"
    );
    ok(
      pocketHeader && !pocketHeader.style.visibility,
      "Pocket header should be visible"
    );

    // Highlights (Recent activity) section.
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("section[data-section-id='highlights']"),
      "Wait for the highlights section to load"
    );

    let highlightsHeader = content.document.querySelector(
      "section[data-section-id='highlights'] .section-title"
    );
    ok(
      highlightsHeader && !highlightsHeader.style.visibility,
      "Highlights header should be visible"
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

// If the pref `browser.newtabpage.activity-stream.newNewtabExperience.enabled`
// and `browser.newtabpage.activity-stream.customizationMenu.enabled` are set to
// false, test that:
// 1. Top sites header is shown and it is collapsible.
// 2. Pocket header and section are visible and the section is collapsible.
// 3. Recent activity section and header are visible and the section is collapsible.
test_newtab({
  async before({ pushPrefs }) {
    await pushPrefs([
      "browser.newtabpage.activity-stream.newNewtabExperience.enabled",
      false,
    ]);
    await pushPrefs([
      "browser.newtabpage.activity-stream.customizationMenu.enabled",
      false,
    ]);
  },
  test: async function test_render_customizeMenu() {
    // Top sites section
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".top-sites"),
      "Wait for the top sites section to load"
    );

    let topSitesHeader = content.document.querySelector(
      ".top-sites .section-title"
    );
    ok(
      topSitesHeader && !topSitesHeader.style.visibility,
      "Top sites header should be visible"
    );

    let topSitesSection = content.document.querySelector(".top-sites");
    let topSitesHeaderClickTarget = content.document.querySelector(
      ".top-sites .section-title .click-target"
    );
    topSitesHeaderClickTarget.click();
    await ContentTaskUtils.waitForCondition(
      () => topSitesSection.className.includes("collapsed"),
      "Top sites section is collapsible on click"
    );

    topSitesHeaderClickTarget.click();
    await ContentTaskUtils.waitForCondition(
      () => !topSitesSection.className.includes("collapsed"),
      "Top sites section is expandable on click"
    );

    // Pocket section
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("section[data-section-id='topstories']"),
      "Wait for the pocket section to load"
    );

    let pocketHeader = content.document.querySelector(
      "section[data-section-id='topstories'] .section-title"
    );
    ok(
      pocketHeader && !pocketHeader.style.visibility,
      "Pocket header should be visible"
    );

    let pocketSection = content.document.querySelector(
      "section[data-section-id='topstories']"
    );
    let pocketHeaderClickTarget = content.document.querySelector(
      "section[data-section-id='topstories'] .section-title .click-target"
    );
    pocketHeaderClickTarget.click();
    await ContentTaskUtils.waitForCondition(
      () => pocketSection.className.includes("collapsed"),
      "Pocket section is collapsible on click"
    );

    pocketHeaderClickTarget.click();
    await ContentTaskUtils.waitForCondition(
      () => !pocketSection.className.includes("collapsed"),
      "Pocket section is expandable on click"
    );

    // Highlights (Recent activity) section.
    await ContentTaskUtils.waitForCondition(
      () =>
        content.document.querySelector("section[data-section-id='highlights']"),
      "Wait for the highlights section to load"
    );

    let highlightsHeader = content.document.querySelector(
      "section[data-section-id='highlights'] .section-title"
    );
    ok(
      highlightsHeader && !highlightsHeader.style.visibility,
      "Highlights header should be visible on load"
    );

    let highlightsSection = content.document.querySelector(
      "section[data-section-id='highlights']"
    );
    let highlightsHeaderClickTarget = content.document.querySelector(
      "section[data-section-id='highlights'] .section-title .click-target"
    );
    highlightsHeaderClickTarget.click();
    await ContentTaskUtils.waitForCondition(
      () => highlightsSection.className.includes("collapsed"),
      "Highlights section is collapsible on click"
    );

    highlightsHeaderClickTarget.click();
    await ContentTaskUtils.waitForCondition(
      () => !highlightsSection.className.includes("collapsed"),
      "Highlights section is expandable on click"
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
