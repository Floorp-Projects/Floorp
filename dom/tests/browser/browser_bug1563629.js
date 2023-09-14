"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const PATH = DIRPATH + "file_postMessage_parent.html";

const URL1 = `https://example.com/${PATH}`;
const URL2 = `https://example.org/${PATH}`;

function listenForCrash(win) {
  function listener(event) {
    ok(false, "a crash occurred");
  }

  win.addEventListener("oop-browser-crashed", listener);
  registerCleanupFunction(() => {
    win.removeEventListener("oop-browser-crashed", listener);
  });
}

add_task(async function () {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
    private: true,
    remote: true,
  });

  listenForCrash(win);

  try {
    let tab = win.gBrowser.selectedTab;
    let browser = tab.linkedBrowser;

    BrowserTestUtils.startLoadingURIString(browser, URL1);
    await BrowserTestUtils.browserLoaded(browser, false, URL1);

    async function loadURL(url) {
      let iframe = content.document.createElement("iframe");
      content.document.body.appendChild(iframe);

      iframe.contentWindow.location = url;
      await new Promise(resolve =>
        iframe.addEventListener("load", resolve, { once: true })
      );

      return iframe.browsingContext;
    }

    function length() {
      return content.length;
    }

    let outer = await SpecialPowers.spawn(browser, [URL2], loadURL);
    let inner = await SpecialPowers.spawn(outer, [URL2], loadURL);

    is(await SpecialPowers.spawn(outer, [], length), 1, "have 1 inner frame");
    is(await SpecialPowers.spawn(browser, [], length), 1, "have 1 outer frame");

    // Send a message from the outer iframe to the inner one.
    //
    // This would've previously crashed the content process that URL2 is running
    // in.
    await SpecialPowers.spawn(outer, [], () => {
      content.frames[0].postMessage("foo", "*");
    });

    // Now send a message from the inner frame to the outer one.
    await SpecialPowers.spawn(inner, [], () => {
      content.parent.postMessage("bar", "*");
    });

    // Assert we've made it this far.
    ok(true, "didn't crash");
  } finally {
    await BrowserTestUtils.closeWindow(win);
  }
});
