ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  DownloadsCommon: "resource:///modules/DownloadsCommon.sys.mjs",
});

const HandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);

const MIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

let SECURE_BASE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  ) + "download_page.html";

/**
 * Waits until a download is triggered.
 * It waits until a prompt is shown,
 * saves and then accepts the dialog.
 * @returns {Promise} Resolved once done.
 */

function shouldTriggerDownload() {
  return new Promise((resolve, reject) => {
    Services.wm.addListener({
      onOpenWindow(xulWin) {
        Services.wm.removeListener(this);
        let win = xulWin.docShell.domWindow;
        waitForFocus(() => {
          if (
            win.location ==
            "chrome://mozapps/content/downloads/unknownContentType.xhtml"
          ) {
            let dialog = win.document.getElementById("unknownContentType");
            let button = dialog.getButton("accept");
            let actionRadio = win.document.getElementById("save");
            actionRadio.click();
            button.disabled = false;
            dialog.acceptDialog();
            resolve();
          } else {
            reject();
          }
        }, win);
      },
    });
  });
}

const CONSOLE_UPGRADE_MESSAGE = "Upgrading insecure request";
const CONSOLE_DOWNGRADE_MESSAGE = "Downgrading to “http” again.";
const DOWNLOAD_URL =
  "example.com/browser/dom/security/test/https-first/download_server.sjs";
// Verifies that https-first tries to upgrade download,
// falls back since download is not available via https
let msgCounter = 0;
function shouldConsoleError() {
  return new Promise(resolve => {
    function listener(msgObj) {
      let text = msgObj.message;
      if (text.includes(CONSOLE_UPGRADE_MESSAGE) && msgCounter == 0) {
        ok(
          text.includes("http://" + DOWNLOAD_URL),
          "Https-first tries to upgrade download to https"
        );
        msgCounter++;
      }
      if (text.includes(CONSOLE_DOWNGRADE_MESSAGE) && msgCounter == 1) {
        ok(
          text.includes("https://" + DOWNLOAD_URL),
          "Https-first downgrades download to http."
        );
        resolve();
        Services.console.unregisterListener(listener);
      }
    }
    Services.console.registerListener(listener);
  });
}

async function resetDownloads() {
  // Removes all downloads from the download List
  const types = new Set();
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    if (download.contentType) {
      types.add(download.contentType);
    }
    publicList.remove(download);
    await download.finalize(true);
  }

  if (types.size) {
    // reset handlers for the contentTypes of any files previously downloaded
    for (let type of types) {
      const mimeInfo = MIMEService.getFromTypeAndExtension(type, "");
      info("resetting handler for type: " + type);
      HandlerService.remove(mimeInfo);
    }
  }
}

async function runTest(url, link, checkFunction, description) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", true],
      ["browser.download.always_ask_before_handling_new_types", true],
    ],
  });
  requestLongerTimeout(2);
  await resetDownloads();

  let tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  is(
    gBrowser.currentURI.schemeIs("https"),
    true,
    "Scheme of opened tab should be https"
  );
  info("Checking: " + description);

  let checkPromise = checkFunction();
  // Click the Link to trigger the download
  SpecialPowers.spawn(gBrowser.selectedBrowser, [link], contentLink => {
    content.document.getElementById(contentLink).click();
  });
  await checkPromise;
  ok(true, description);
  BrowserTestUtils.removeTab(tab);
}

//Test description:
// 1. Open "https://example.com"
// 2. From "https://example.com" download something, but that download is only available via http.
// 3. Https-first tries to upgrade the download.
// 4. Upgrading fails - so http-first downgrade download to http.

add_task(async function test_mixed_download() {
  await runTest(
    SECURE_BASE_URL,
    "insecure",
    () => Promise.all([shouldTriggerDownload(), shouldConsoleError()]),
    "Secure -> Insecure should Error"
  );
  // remove downloaded file
  let downloadsPromise = Downloads.getList(Downloads.PUBLIC);
  let downloadList = await downloadsPromise;
  let [download] = downloadList._downloads;
  await downloadList.remove(download);
});
