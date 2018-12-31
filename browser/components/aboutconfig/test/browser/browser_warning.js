/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.aboutConfig.showWarning", true],
    ],
  });
});

add_task(async function test_load_warningpage() {
  await AboutConfigTest.withNewTab(async function() {
    // Test that the warning page is presented:
    Assert.equal(this.document.getElementsByTagName("button").length, 1);
    Assert.equal(this.document.getElementById("search"), undefined);
    Assert.equal(this.document.getElementById("prefs"), undefined);

    // Disable checkbox and reload.
    this.document.getElementById("showWarningNextTime").click();
    this.document.querySelector("button").click();
  }, { dontBypassWarning: true });

  await AboutConfigTest.withNewTab(async function() {
    Assert.ok(this.document.getElementById("search"));
    Assert.ok(this.document.getElementById("prefs"));
  }, { dontBypassWarning: true });
});
