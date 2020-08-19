let INSECURE_BASE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://example.com/"
  ) + "download_page.html";
let SECURE_BASE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "https://example.com/"
  ) + "download_page.html";

function shouldPromptDownload() {
  // Waits until the download Prompt is shown
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
            resolve();
            info("Trying to close window");
            win.close();
          } else {
            reject();
          }
        }, win);
      },
    });
  });
}

const CONSOLE_ERROR_MESSAGE = "Blocked downloading insecure content";

function shouldConsoleError() {
  // Waits until CONSOLE_ERROR_MESSAGE was logged
  return new Promise((resolve, reject) => {
    function listener(msgObj) {
      let text = msgObj.message;
      if (text.includes(CONSOLE_ERROR_MESSAGE)) {
        Services.console.unregisterListener(listener);
        resolve();
      }
    }
    Services.console.registerListener(listener);
  });
}

async function runTest(url, link, checkFunction, decscription) {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.block_download_insecure", true]],
  });

  let tab = BrowserTestUtils.addTab(gBrowser, url);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Checking: " + decscription);

  let checkPromise = checkFunction();
  // Click the Link to trigger the download
  SpecialPowers.spawn(gBrowser.selectedBrowser, [link], contentLink => {
    content.document.getElementById(contentLink).click();
  });

  await checkPromise;

  ok(true, decscription);
  BrowserTestUtils.removeTab(tab);

  await SpecialPowers.popPrefEnv();
}

add_task(async function() {
  await runTest(
    INSECURE_BASE_URL,
    "insecure",
    shouldPromptDownload,
    "Insecure -> Insecure should download"
  );
  await runTest(
    INSECURE_BASE_URL,
    "secure",
    shouldPromptDownload,
    "Insecure -> Secure should download"
  );
  await runTest(
    SECURE_BASE_URL,
    "insecure",
    shouldConsoleError,
    "Secure -> Insecure should Error"
  );
  await runTest(
    SECURE_BASE_URL,
    "secure",
    shouldPromptDownload,
    "Secure -> Secure should Download"
  );
});
