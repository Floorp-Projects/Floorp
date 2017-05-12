/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const uuidGenerator =
  Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

const DUMMY_FILE = "dummy_page.html";

// Test for bug 1327942.
add_task(async function() {
  // Copy dummy page to unique file in TmpD, so that we can safely delete it.
  let dummyPage = getChromeDir(getResolvedURI(gTestPath));
  dummyPage.append(DUMMY_FILE);
  let disappearingPage = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let uniqueName = uuidGenerator.generateUUID().toString();
  dummyPage.copyTo(disappearingPage, uniqueName);
  disappearingPage.append(uniqueName);

  // Get file:// URI for new page and load in a new tab.
  const uriString = Services.io.newFileURI(disappearingPage).spec;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, uriString);
  registerCleanupFunction(async function() {
    await BrowserTestUtils.removeTab(tab);
  });

  // Delete the page, simulate a click of the reload button and check that we
  // get a neterror page.
  disappearingPage.remove(false);
  document.getElementById("reload-button").doCommand();
  await BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    ok(content.document.documentURI.startsWith("about:neterror"),
       "Check that a neterror page was loaded.");
  });
});
