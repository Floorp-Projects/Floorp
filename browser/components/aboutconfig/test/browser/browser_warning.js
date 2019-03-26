/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is a temporary workaround to
 * be resolved in bug 1539000.
 */
ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", this);
PromiseTestUtils.whitelistRejectionsGlobally(/Too many characters in placeable/);

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.aboutConfig.showWarning", true],
    ],
  });
});

add_task(async function test_showWarningNextTime() {
  for (let test of [
    { expectWarningPage: true, disableShowWarningNextTime: false },
    { expectWarningPage: true, disableShowWarningNextTime: true },
    { expectWarningPage: false },
  ]) {
    await AboutConfigTest.withNewTab(async function() {
      if (test.expectWarningPage) {
        this.assertWarningPage(true);
        Assert.ok(this.document.getElementById("showWarningNextTime").checked);
        if (test.disableShowWarningNextTime) {
          this.document.getElementById("showWarningNextTime").click();
        }
        this.bypassWarningButton.click();
      }

      // No results are shown after the warning page is dismissed or bypassed.
      this.assertWarningPage(false);
      Assert.ok(!this.prefsTable.firstElementChild);
      Assert.equal(this.document.activeElement, this.searchInput);

      // The show all button should be present and show all results immediately.
      this.showAll();
      Assert.ok(this.prefsTable.firstElementChild);
    }, { dontBypassWarning: true });
  }
});
