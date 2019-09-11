/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "test.aboutconfig.userValueLikeLocalized",
        "chrome://test/locale/testing.properties",
      ],
    ],
  });
});

add_task(async function test_load_title() {
  await AboutConfigTest.withNewTab(async function() {
    Assert.equal(this.document.title, "Advanced Configurations");
  });
});

add_task(async function test_load_settings() {
  await AboutConfigTest.withNewTab(async function() {
    // Test if page contains elements.
    Assert.equal(this.getRow(PREF_NUMBER_DEFAULT_ZERO).value, 0);
    Assert.equal(this.getRow(PREF_STRING_DEFAULT_EMPTY).value, "");

    // Test if the modified state is displayed for the right prefs.
    Assert.ok(
      !this.getRow(PREF_BOOLEAN_DEFAULT_TRUE).hasClass("has-user-value")
    );
    Assert.ok(
      this.getRow(PREF_BOOLEAN_USERVALUE_TRUE).hasClass("has-user-value")
    );

    // Test to see if values are localized, sampling from different files. If
    // any of these are removed or their value changes, just update the value
    // here or point to a different preference in the same file.
    Assert.equal(this.getRow("font.language.group").value, "x-western");
    Assert.equal(this.getRow("intl.ellipsis").value, "\u2026");
    Assert.equal(
      this.getRow("gecko.handlerService.schemes.mailto.1.uriTemplate").value,
      "https://mail.google.com/mail/?extsrc=mailto&url=%s"
    );

    // Test to see if user created value is not empty string when it matches
    // /^chrome:\/\/.+\/locale\/.+\.properties/.
    Assert.equal(
      this.getRow("test.aboutconfig.userValueLikeLocalized").value,
      "chrome://test/locale/testing.properties"
    );

    // Test to see if empty string when value matches
    // /^chrome:\/\/.+\/locale\/.+\.properties/ and an exception is thrown.
    Assert.equal(this.getRow(PREF_STRING_LOCALIZED_MISSING).value, "");
  });
});
