/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Checks that if browser.fixup.dns_first_for_single_words pref is set, we pass
// the original search string to the docshell and not a search url.

add_task(async function test() {
  const { sinon } = ChromeUtils.importESModule(
    "resource://testing-common/Sinon.sys.mjs"
  );
  const sandbox = sinon.createSandbox();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.fixup.dns_first_for_single_words", true]],
  });

  registerCleanupFunction(sandbox.restore);

  /**
   * Tests the given search string.
   *
   * @param {string} str The search string
   * @param {boolean} passthrough whether the value should be passed unchanged
   * to the docshell that will first execute a DNS request.
   */
  async function testVal(str, passthrough) {
    sandbox.stub(gURLBar, "_loadURL").callsFake(url => {
      if (passthrough) {
        Assert.equal(url, str, "Should pass the unmodified search string");
      } else {
        Assert.ok(url.startsWith("http"), "Should pass an url");
      }
    });
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: str,
    });
    EventUtils.synthesizeKey("KEY_Enter");
    sandbox.restore();
  }

  await testVal("test", true);
  await testVal("te-st", true);
  await testVal("test ", true);
  await testVal(" test", true);
  await testVal(" test", true);
  await testVal("test.test", true);
  await testVal("test test", false);
  // This is not a single word host, though it contains one. At a certain point
  // we may evaluate to increase coverage of the feature to also ask for this.
  await testVal("test/test", false);
});
