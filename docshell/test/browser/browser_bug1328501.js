const HTML_URL = "http://mochi.test:8888/browser/docshell/test/browser/file_bug1328501.html";
const FRAME_URL = "http://mochi.test:8888/browser/docshell/test/browser/file_bug1328501_frame.html";
const FRAME_SCRIPT_URL = "chrome://mochitests/content/browser/docshell/test/browser/file_bug1328501_framescript.js";
add_task(async function testMultiFrameRestore() {
  await BrowserTestUtils.withNewTab({gBrowser, url: HTML_URL}, async function(browser) {
    // Navigate 2 subframes and load about:blank.
    let browserLoaded = BrowserTestUtils.browserLoaded(browser);
    await ContentTask.spawn(browser, FRAME_URL, async function(FRAME_URL) {
      function frameLoaded(frame) {
        frame.contentWindow.location = FRAME_URL;
        return new Promise(r => frame.onload = r);
      }
      let frame1 = content.document.querySelector("#testFrame1");
      let frame2 = content.document.querySelector("#testFrame2");
      ok(frame1, "check found testFrame1");
      ok(frame2, "check found testFrame2");
      await frameLoaded(frame1);
      await frameLoaded(frame2);
      content.location = "dummy_page.html";
    });
    await browserLoaded;

    // Load a frame script to query nsIDOMWindow on "http-on-opening-request",
    // which will force about:blank content viewers being created.
    browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);

    // The frame script also forwards frames-loaded.
    let framesLoaded = BrowserTestUtils.waitForMessage(browser.messageManager, "test:frames-loaded");

    browser.goBack();
    await framesLoaded;
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 1000));
    await ContentTask.spawn(browser, FRAME_URL, (FRAME_URL) => {
      is(content.document.querySelector("#testFrame1").contentWindow.location.href, FRAME_URL);
      is(content.document.querySelector("#testFrame2").contentWindow.location.href, FRAME_URL);
    });
  });
});
