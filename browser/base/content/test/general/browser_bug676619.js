var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

function waitForNewWindow() {
  return new Promise(resolve => {
    var listener = {
      onOpenWindow: aXULWindow => {
        info("Download window shown...");
        Services.wm.removeListener(listener);

        function downloadOnLoad() {
          domwindow.removeEventListener("load", downloadOnLoad, true);

          is(
            domwindow.document.location.href,
            "chrome://mozapps/content/downloads/unknownContentType.xhtml",
            "Download page appeared"
          );
          resolve(domwindow);
        }

        var domwindow = aXULWindow.docShell.domWindow;
        domwindow.addEventListener("load", downloadOnLoad, true);
      },
      onCloseWindow: aXULWindow => {},
    };

    Services.wm.addListener(listener);
  });
}

async function waitForFilePickerTest(link, name) {
  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = function(fp) {
      ok(true, "Filepicker shown.");
      is(name, fp.defaultString, " filename matches download name");
      setTimeout(resolve, 0);
      return Ci.nsIFilePicker.returnCancel;
    };
  });

  SpecialPowers.spawn(gBrowser.selectedBrowser, [link], contentLink => {
    content.document.getElementById(contentLink).click();
  });

  await filePickerShownPromise;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    Assert.equal(
      content.document.getElementById("unload-flag").textContent,
      "Okay",
      "beforeunload shouldn't have fired"
    );
  });
}

async function testLink(link, name) {
  info("Checking " + link + " with name: " + name);

  if (
    Services.prefs.getBoolPref(
      "browser.download.improvements_to_download_panel"
    )
  ) {
    await waitForFilePickerTest(link, name);
    return;
  }

  let winPromise = waitForNewWindow();

  SpecialPowers.spawn(gBrowser.selectedBrowser, [link], contentLink => {
    content.document.getElementById(contentLink).click();
  });

  let win = await winPromise;

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    Assert.equal(
      content.document.getElementById("unload-flag").textContent,
      "Okay",
      "beforeunload shouldn't have fired"
    );
  });

  is(
    win.document.getElementById("location").value,
    name,
    `file name should match (${link})`
  );

  await BrowserTestUtils.closeWindow(win);
}

// Cross-origin URL does not trigger a download
async function testLocation(link, url) {
  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

  SpecialPowers.spawn(gBrowser.selectedBrowser, [link], contentLink => {
    content.document.getElementById(contentLink).click();
  });

  let tab = await tabPromise;
  BrowserTestUtils.removeTab(tab);
}

async function runTest(url) {
  let tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await testLink("link1", "test.txt");
  await testLink("link2", "video.ogg");
  await testLink("link3", "just some video.ogg");
  await testLink("link4", "with-target.txt");
  await testLink("link5", "javascript.html");
  await testLink("link6", "test.blob");
  await testLink("link7", "test.file");
  await testLink("link8", "download_page_3.txt");
  await testLink("link9", "download_page_3.txt");
  await testLink("link10", "download_page_4.txt");
  await testLink("link11", "download_page_4.txt");
  await testLocation("link12", "http://example.com/");

  // Check that we enforce the correct extension if the website's
  // is bogus or missing. These extensions can differ slightly (ogx vs ogg,
  // htm vs html) on different OSes.
  let oggExtension = getMIMEInfoForType("application/ogg").primaryExtension;
  await testLink("link13", "no file extension." + oggExtension);

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1690051#c8
  if (AppConstants.platform != "win") {
    const PREF = "browser.download.sanitize_non_media_extensions";
    ok(Services.prefs.getBoolPref(PREF), "pref is set before");

    // Check that ics (iCal) extension is changed/fixed when the pref is true.
    await testLink("link14", "dummy.ics");

    // And not changed otherwise.
    Services.prefs.setBoolPref(PREF, false);
    await testLink("link14", "dummy.not-ics");
    Services.prefs.clearUserPref(PREF);
  }

  await testLink("link15", "download_page_3.txt");
  await testLink("link16", "download_page_3.txt");
  await testLink("link17", "download_page_4.txt");
  await testLink("link18", "download_page_4.txt");

  BrowserTestUtils.removeTab(tab);
}

async function setDownloadDir() {
  let tmpDir = await PathUtils.getTempDir();
  tmpDir = PathUtils.join(
    tmpDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function() {
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      Cu.reportError(e);
    }
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
}

add_task(async function() {
  requestLongerTimeout(3);
  waitForExplicitFinish();

  await setDownloadDir();

  info("Test with browser.download.improvements_to_download_panel enabled.");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", true],
      ["browser.download.useDownloadDir", false],
    ],
  });

  await runTest(
    "http://mochi.test:8888/browser/browser/base/content/test/general/download_page.html"
  );
  await runTest(
    "https://example.com:443/browser/browser/base/content/test/general/download_page.html"
  );

  info("Test with browser.download.improvements_to_download_panel disabled.");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.improvements_to_download_panel", false],
      ["browser.download.useDownloadDir", true],
    ],
  });

  await runTest(
    "http://mochi.test:8888/browser/browser/base/content/test/general/download_page.html"
  );
  await runTest(
    "https://example.com:443/browser/browser/base/content/test/general/download_page.html"
  );

  MockFilePicker.cleanup();
});
