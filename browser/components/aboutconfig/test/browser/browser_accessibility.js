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
      ["test.aboutconfig.added", true],
    ],
  });
});

add_task(async function test_accessible_value() {
  await AboutConfigTest.withNewTab(async function() {
    for (let [name, expectHasUserValue] of [
      [PREF_BOOLEAN_DEFAULT_TRUE, false],
      [PREF_BOOLEAN_USERVALUE_TRUE, true],
      ["test.aboutconfig.added", true],
    ]) {
      let span = this.getRow(name).valueCell.querySelector("span");
      let expectedL10nId = expectHasUserValue
          ? "about-config-pref-accessible-value-custom"
          : "about-config-pref-accessible-value-default";
      Assert.equal(span.getAttribute("data-l10n-id"), expectedL10nId);
    }
  });
});
