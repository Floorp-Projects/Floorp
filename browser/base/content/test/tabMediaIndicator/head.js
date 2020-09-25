/**
 * Return a web-based URL for a given file based on the testing directory.
 * @param {String} fileName
 *        file that caller wants its web-based url
 * @param {Boolean} cors [optional]
 *        if set, then return a url with different origin
 */
function GetTestWebBasedURL(fileName, cors = false) {
  const origin = cors ? "http://example.org" : "http://example.com";
  return (
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    fileName
  );
}

/**
 * Wait until tab sound indicator appears on the given tab.
 * @param {tabbrowser} tab
 *        given tab where tab sound indicator should appear
 */
async function waitForTabSoundIndicatorAppears(tab) {
  if (!tab.soundPlaying) {
    info("Tab sound indicator doesn't appear yet");
    await BrowserTestUtils.waitForEvent(
      tab,
      "TabAttrModified",
      false,
      event => {
        return event.detail.changed.includes("soundplaying");
      }
    );
  }
  ok(tab.soundPlaying, "Tab sound indicator appears");
}

/**
 * Wait until tab sound indicator disappears on the given tab.
 * @param {tabbrowser} tab
 *        given tab where tab sound indicator should disappear
 */
async function waitForTabSoundIndicatorDisappears(tab) {
  if (tab.soundPlaying) {
    info("Tab sound indicator doesn't disappear yet");
    await BrowserTestUtils.waitForEvent(
      tab,
      "TabAttrModified",
      false,
      event => {
        return event.detail.changed.includes("soundplaying");
      }
    );
  }
  ok(!tab.soundPlaying, "Tab sound indicator disappears");
}
