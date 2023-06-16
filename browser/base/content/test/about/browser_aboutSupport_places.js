/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_places_db_stats_table() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:support" },
    async function (browser) {
      const [initialToggleText, toggleTextAfterShow, toggleTextAfterHide] =
        await SpecialPowers.spawn(browser, [], async function () {
          const toggleButton = content.document.getElementById(
            "place-database-stats-toggle"
          );
          const getToggleText = () =>
            content.document.l10n.getAttributes(toggleButton).id;
          const toggleTexts = [];
          const table = content.document.getElementById(
            "place-database-stats-tbody"
          );
          await ContentTaskUtils.waitForCondition(
            () => table.style.display === "none",
            "Stats table is hidden initially"
          );
          toggleTexts.push(getToggleText());
          toggleButton.click();
          await ContentTaskUtils.waitForCondition(
            () => table.style.display === "",
            "Stats table is shown after first toggle"
          );
          toggleTexts.push(getToggleText());
          toggleButton.click();
          await ContentTaskUtils.waitForCondition(
            () => table.style.display === "none",
            "Stats table is hidden after second toggle"
          );
          toggleTexts.push(getToggleText());
          return toggleTexts;
        });
      Assert.equal(initialToggleText, "place-database-stats-show");
      Assert.equal(toggleTextAfterShow, "place-database-stats-hide");
      Assert.equal(toggleTextAfterHide, "place-database-stats-show");
    }
  );
});
