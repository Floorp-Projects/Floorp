XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";

function getSupportsFile(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  let uri = Services.io.newURI(CHROME_URL_ROOT + path);
  let fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

function installAddon(xpiName) {
  let filePath = getSupportsFile(`addons/${xpiName}`).file;
  return new Promise((resolve, reject) => {
    AddonManager.getInstallForFile(filePath, install => {
      if (!install) {
        throw new Error(`An install was not created for ${filePath}`);
      }
      install.addListener({
        onDownloadFailed: reject,
        onDownloadCancelled: reject,
        onInstallFailed: reject,
        onInstallCancelled: reject,
        onInstallEnded: resolve
      });
      install.install();
    });
  });
}

function waitForMessageChange(messageId, cb) {
  return new Promise((resolve) => {
    let target = gBrowser.contentDocument.getElementById(messageId);
    let observer = new MutationObserver(() => {
      if (cb(target)) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(target, { attributes: true, attributeFilter: ["hidden"] });
  });
}

function waitForMessageHidden(messageId) {
  return waitForMessageChange(messageId, target => target.hidden);
}

function waitForMessageShown(messageId) {
  return waitForMessageChange(messageId, target => !target.hidden);
}

add_task(async function testExtensionControlledHomepage() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  is(gBrowser.currentURI.spec, "about:preferences#general",
     "#general should be in the URI for about:preferences");
  let homepagePref = () => Services.prefs.getCharPref("browser.startup.homepage");
  let originalHomepagePref = homepagePref();
  let extensionHomepage = "https://developer.mozilla.org/";
  let controlledContent = doc.getElementById("browserHomePageExtensionContent");

  // The homepage is set to the default and editable.
  ok(originalHomepagePref != extensionHomepage, "homepage is empty by default");
  is(doc.getElementById("browserHomePage").disabled, false, "The homepage input is enabled");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the homepage.
  await installAddon("set_homepage.xpi");
  await waitForMessageShown("browserHomePageExtensionContent");

  // The homepage has been set by the extension, the user is notified and it isn't editable.
  let controlledLabel = controlledContent.querySelector("description");
  is(homepagePref(), extensionHomepage, "homepage is set by extension");
  // There are two spaces before "set_homepage" because it's " <image /> set_homepage".
  is(controlledLabel.textContent, "An extension,  set_homepage, controls your home page.",
     "The user is notified that an extension is controlling the homepage");
  is(controlledContent.hidden, false, "The extension controlled row is hidden");
  is(doc.getElementById("browserHomePage").disabled, true, "The homepage input is disabled");

  // Disable the extension.
  doc.getElementById("disableHomePageExtension").click();

  await waitForMessageHidden("browserHomePageExtensionContent");

  is(homepagePref(), originalHomepagePref, "homepage is set back to default");
  is(doc.getElementById("browserHomePage").disabled, false, "The homepage input is enabled");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testExtensionControlledNewTab() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  is(gBrowser.currentURI.spec, "about:preferences#general",
     "#general should be in the URI for about:preferences");

  let controlledContent = doc.getElementById("browserNewTabExtensionContent");

  // The new tab is set to the default and message is hidden.
  ok(!aboutNewTabService.newTabURL.startsWith("moz-extension:"), "new tab is not set");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  // Install an extension that will set the new tab page.
  await installAddon("set_newtab.xpi");

  await waitForMessageShown("browserNewTabExtensionContent");

  // The new tab page has been set by the extension and the user is notified.
  let controlledLabel = controlledContent.querySelector("description");
  ok(aboutNewTabService.newTabURL.startsWith("moz-extension:"), "new tab url is set by extension");
  // There are two spaces before "set_newtab" because it's " <image /> set_newtab".
  is(controlledLabel.textContent, "An extension,  set_newtab, controls your new tab page.",
     "The user is notified that an extension is controlling the new tab page");
  is(controlledContent.hidden, false, "The extension controlled row is hidden");

  // Disable the extension.
  doc.getElementById("disableNewTabExtension").click();

  await waitForMessageHidden("browserNewTabExtensionContent");

  ok(!aboutNewTabService.newTabURL.startsWith("moz-extension:"), "new tab page is set back to default");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

