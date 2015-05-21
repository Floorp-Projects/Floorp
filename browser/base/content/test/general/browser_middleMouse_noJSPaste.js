/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const middleMousePastePref = "middlemouse.contentLoadURL";
const autoScrollPref = "general.autoScroll";

add_task(function* () {
  yield pushPrefs([middleMousePastePref, true], [autoScrollPref, false]);

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser);

  let url = "javascript:http://www.example.com/";
  yield new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(url, () => {
      Components.classes["@mozilla.org/widget/clipboardhelper;1"]
                .getService(Components.interfaces.nsIClipboardHelper)
                .copyString(url);
    }, resolve, () => {
      ok(false, "Clipboard copy failed");
      reject();
    });
  });

  let middlePagePromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Middle click on the content area
  info("Middle clicking");
  yield BrowserTestUtils.synthesizeMouse(null, 10, 10, {button: 1}, gBrowser.selectedBrowser);
  yield middlePagePromise;

  is(gBrowser.currentURI.spec, url.replace(/^javascript:/, ""), "url loaded by middle click doesn't include JS");

  gBrowser.removeTab(tab);
});
