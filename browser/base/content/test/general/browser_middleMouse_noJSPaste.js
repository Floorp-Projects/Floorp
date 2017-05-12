/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const middleMousePastePref = "middlemouse.contentLoadURL";
const autoScrollPref = "general.autoScroll";

add_task(async function() {
  await pushPrefs([middleMousePastePref, true], [autoScrollPref, false]);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let url = "javascript:http://www.example.com/";
  await new Promise((resolve, reject) => {
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
  await BrowserTestUtils.synthesizeMouse(null, 10, 10, {button: 1}, gBrowser.selectedBrowser);
  await middlePagePromise;

  is(gBrowser.currentURI.spec, url.replace(/^javascript:/, ""), "url loaded by middle click doesn't include JS");

  gBrowser.removeTab(tab);
});
