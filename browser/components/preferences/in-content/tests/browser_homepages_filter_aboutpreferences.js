add_task(async function testSetHomepageUseCurrent() {
  is(gBrowser.currentURI.spec, "about:blank", "Test starts with about:blank open");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  is(gBrowser.currentURI.spec, "about:preferences#general",
     "#general should be in the URI for about:preferences");
  let oldHomepagePref = Services.prefs.getCharPref("browser.startup.homepage");

  let useCurrent = doc.getElementById("useCurrent");
  useCurrent.click();

  is(gBrowser.tabs.length, 3, "Three tabs should be open");
  is(Services.prefs.getCharPref("browser.startup.homepage"), "about:blank|about:home",
     "about:blank and about:home should be the only homepages set");

  Services.prefs.setCharPref("browser.startup.homepage", oldHomepagePref);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";

function getSupportsFile(path) {
  let cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  let uri = Services.io.newURI(CHROME_URL_ROOT + path);
  let fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

function installAddon() {
  let filePath = getSupportsFile("addons/set_homepage.xpi").file;
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

function waitForMessageChange(cb) {
  return new Promise((resolve) => {
    let target = gBrowser.contentDocument.getElementById("browserHomePageExtensionContent");
    let observer = new MutationObserver(() => {
      if (cb(target)) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(target, { attributes: true, attributeFilter: ["hidden"] });
  });
}

function waitForMessageHidden() {
  return waitForMessageChange(target => target.hidden);
}

function waitForMessageShown() {
  return waitForMessageChange(target => !target.hidden);
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
  await installAddon();
  await waitForMessageShown();

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

  await waitForMessageHidden();

  is(homepagePref(), originalHomepagePref, "homepage is set back to default");
  is(doc.getElementById("browserHomePage").disabled, false, "The homepage input is enabled");
  is(controlledContent.hidden, true, "The extension controlled row is hidden");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
