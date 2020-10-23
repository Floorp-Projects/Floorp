/**
 * Global variables for testing.
 */
const gEMPTY_PAGE_URL = GetTestWebBasedURL("file_empty.html");

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

/**
 * Return a new foreground tab loading with an empty file.
 * @param needObserver
 *        If true, sets an observer property on the returned tab. This property
 *        exposes `hasEverUpdated()` which will return a bool indicating if the
 *        sound indicator has ever updated.
 */
async function createBlankForegroundTab({ needObserver } = {}) {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    gEMPTY_PAGE_URL
  );
  if (needObserver) {
    tab.observer = createSoundIndicatorObserver(tab);
  }
  return tab;
}

function createSoundIndicatorObserver(tab) {
  let hasEverUpdated = false;
  let listener = event => {
    if (event.detail.changed.includes("soundplaying")) {
      hasEverUpdated = true;
    }
  };
  tab.addEventListener("TabAttrModified", listener);
  return {
    hasEverUpdated: () => {
      tab.removeEventListener("TabAttrModified", listener);
      return hasEverUpdated;
    },
  };
}
