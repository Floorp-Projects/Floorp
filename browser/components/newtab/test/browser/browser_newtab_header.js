"use strict";

// Tests that:
// 1. Top sites header is hidden and the topsites section is not collapsed on load.
// 2. Pocket header and section are visible and not collapsed on load.
// 3. Recent activity section and header are visible and not collapsed on load.
test_newtab({
  test: async function test_render_customizeMenu() {
    // Top sites section
    await ContentTaskUtils.waitForCondition(
      () => content.document.querySelector(".top-sites"),
      "Wait for the top sites section to load"
    );

    let topSitesSection = content.document.querySelector(".top-sites");
    let titleContainer = topSitesSection.querySelector(
      ".section-title-container"
    );
    ok(
      titleContainer && titleContainer.style.visibility === "hidden",
      "Top sites header should  not be visible"
    );

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
    let isPocketSectionCollapsed =
      pocketSection.className.includes("collapsed");
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
    let isHighlightsSectionCollapsed =
      highlightsSection.className.includes("collapsed");
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
});
