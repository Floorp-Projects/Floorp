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
    let list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    function getRow(name) {
      return list.find(row => row.querySelector("td").textContent == name);
    }
    function getValue(name) {
      return getRow(name).querySelector("td.cell-value").textContent;
    }

    // Test if page contains elements.
    Assert.ok(getRow("plugins.testmode"));
    Assert.ok(getRow("dom.vr.enabled"));
    Assert.ok(getRow("accessibility.AOM.enabled"));

    // Test if the modified state is displayed for the right prefs.
    let prefArray = Services.prefs.getChildList("");
    let nameOfEdited = prefArray.find(
      name => Services.prefs.prefHasUserValue(name));
    let nameOfDefault = prefArray.find(
      name => !Services.prefs.prefHasUserValue(name));
    Assert.ok(!getRow(nameOfDefault).classList.contains("has-user-value"));
    Assert.ok(getRow(nameOfEdited).classList.contains("has-user-value"));

    // Test to see if values are localized.
    Assert.equal(getValue("font.language.group"), "x-western");
    Assert.equal(getValue("intl.ellipsis"), "\u2026");
    Assert.equal(
      getValue("gecko.handlerService.schemes.mailto.1.uriTemplate"),
      "https://mail.google.com/mail/?extsrc=mailto&url=%s");

    // Test to see if user created value is not empty string when it matches /^chrome:\/\/.+\/locale\/.+\.properties/.
    Assert.equal(getValue("random.user.pref"),
      "chrome://test/locale/testing.properties");
    // Test to see if empty string when value matches /^chrome:\/\/.+\/locale\/.+\.properties/ and an exception is thrown.
    Assert.equal(getValue("gecko.handlerService.schemes.irc.1.name"), "");
  });
});
