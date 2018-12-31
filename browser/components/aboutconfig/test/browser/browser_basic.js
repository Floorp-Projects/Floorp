/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["random.user.pref", "chrome://test/locale/testing.properties"],
    ],
  });
});

add_task(async function test_load_title() {
  await AboutConfigTest.withNewTab(async function() {
    Assert.equal(this.document.title, "about:config");
  });
});

add_task(async function test_load_settings() {
  await AboutConfigTest.withNewTab(async function() {
    // Test if page contains elements.
    Assert.ok(this.getRow("plugins.testmode"));
    Assert.ok(this.getRow("dom.vr.enabled"));
    Assert.ok(this.getRow("accessibility.AOM.enabled"));

    // Test if the modified state is displayed for the right prefs.
    let prefArray = Services.prefs.getChildList("");
    let nameOfEdited = prefArray.find(
      name => Services.prefs.prefHasUserValue(name));
    let nameOfDefault = prefArray.find(
      name => !Services.prefs.prefHasUserValue(name));
    Assert.ok(!this.getRow(nameOfDefault).hasClass("has-user-value"));
    Assert.ok(this.getRow(nameOfEdited).hasClass("has-user-value"));

    // Test to see if values are localized.
    Assert.equal(this.getRow("font.language.group").value, "x-western");
    Assert.equal(this.getRow("intl.ellipsis").value, "\u2026");
    Assert.equal(
      this.getRow("gecko.handlerService.schemes.mailto.1.uriTemplate").value,
      "https://mail.google.com/mail/?extsrc=mailto&url=%s");

    // Test to see if user created value is not empty string when it matches
    // /^chrome:\/\/.+\/locale\/.+\.properties/.
    Assert.equal(this.getRow("random.user.pref").value,
      "chrome://test/locale/testing.properties");

    // Test to see if empty string when value matches
    // /^chrome:\/\/.+\/locale\/.+\.properties/ and an exception is thrown.
    Assert.equal(this.getRow("gecko.handlerService.schemes.irc.1.name").value,
      "");
  });
});
