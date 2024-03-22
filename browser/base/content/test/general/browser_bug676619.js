var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window.browsingContext);

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
      onCloseWindow: () => {},
    };

    Services.wm.addListener(listener);
    registerCleanupFunction(() => {
      try {
        Services.wm.removeListener(listener);
      } catch (e) {}
    });
  });
}

async function waitForFilePickerTest(link, name) {
  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = function (fp) {
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
      "browser.download.always_ask_before_handling_new_types",
      false
    )
  ) {
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
  } else {
    await waitForFilePickerTest(link, name);
  }
}

// Cross-origin URL does not trigger a download
async function testLocation(link) {
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
  await testLink("link2", "video.webm");
  await testLink("link3", "just some video.webm");
  await testLink("link4", "with-target.txt");
  await testLink("link5", "javascript.html");
  await testLink("link6", "test.blob");
  await testLink("link7", "test.file");
  await testLink("link8", "download_page_3.txt");
  await testLink("link9", "download_page_3.txt");
  await testLink("link10", "download_page_4.txt");
  await testLink("link11", "download_page_4.txt");
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await testLocation("link12", "http://example.com/");

  // Check that we enforce the correct extension if the website's
  // is bogus or missing. These extensions can differ slightly (ogx vs ogg,
  // htm vs html) on different OSes.
  let webmExtension = getMIMEInfoForType("video/webm").primaryExtension;
  await testLink("link13", "no file extension." + webmExtension);

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
  await testLink("link19", "download_page_4.txt");
  await testLink("link20", "download_page_4.txt");
  await testLink("link21", "download_page_4.txt");
  await testLink("link22", "download_page_4.txt");

  BrowserTestUtils.removeTab(tab);
}

async function setDownloadDir() {
  let tmpDir = PathUtils.join(
    PathUtils.tempDir,
    "testsavedir" + Math.floor(Math.random() * 2 ** 32)
  );
  // Create this dir if it doesn't exist (ignores existing dirs)
  await IOUtils.makeDirectory(tmpDir);
  registerCleanupFunction(async function () {
    try {
      await IOUtils.remove(tmpDir, { recursive: true });
    } catch (e) {
      console.error(e);
    }
    Services.prefs.clearUserPref("browser.download.folderList");
    Services.prefs.clearUserPref("browser.download.dir");
  });
  Services.prefs.setIntPref("browser.download.folderList", 2);
  Services.prefs.setCharPref("browser.download.dir", tmpDir);
}

add_task(async function () {
  requestLongerTimeout(3);
  waitForExplicitFinish();

  await setDownloadDir();

  info(
    "Test with browser.download.always_ask_before_handling_new_types enabled."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", true],
      ["browser.download.useDownloadDir", true],
    ],
  });

  await runTest(
    "http://mochi.test:8888/browser/browser/base/content/test/general/download_page.html"
  );
  await runTest(
    "https://example.com:443/browser/browser/base/content/test/general/download_page.html"
  );

  info(
    "Test with browser.download.always_ask_before_handling_new_types disabled."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.always_ask_before_handling_new_types", false],
      ["browser.download.useDownloadDir", false],
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
