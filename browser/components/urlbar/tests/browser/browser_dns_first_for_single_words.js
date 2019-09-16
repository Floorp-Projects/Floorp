/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that if browser.fixup.dns_first_for_single_words pref is set, we pass
// the original search string to the docshell and not a search url.

add_task(async function test() {
  const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
  const sandbox = sinon.createSandbox();

  const PREF = "browser.fixup.dns_first_for_single_words";
  Services.prefs.setBoolPref(PREF, true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF);
    sandbox.restore();
  });

  async function testVal(str, passthrough) {
    sandbox.stub(gURLBar, "_loadURL").callsFake(url => {
      if (passthrough) {
        Assert.equal(url, str, "Should pass the unmodified search string");
      } else {
        Assert.ok(url.startsWith("http"), "Should pass an url");
      }
    });
    await promiseAutocompleteResultPopup(str);
    EventUtils.synthesizeKey("KEY_Enter");
    sandbox.restore();
  }

  await testVal("test", true);
  await testVal("te-st", true);
  await testVal("test ", true);
  await testVal(" test", true);
  await testVal(" test", true);
  await testVal("test test", false);
  await testVal("test.test", false);
  await testVal("test/test", false);
});
