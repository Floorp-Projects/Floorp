/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "http://example.com/browser/dom/tests/browser/test_dom_chromeframes_top.html";

add_task(async function() {
  SpecialPowers.pushPrefEnv({set: [["dom.chrome_frame_access.enabled", true]]})
  await testProperties(TEST_URI, true);

  SpecialPowers.pushPrefEnv({set: [["dom.chrome_frame_access.enabled", false]]})
  await testProperties(TEST_URI, false);
});

async function testProperties(uri, shouldBeDefined) {
  await BrowserTestUtils.withNewTab(uri, async function(browser) {
    // eslint-disable-next-line no-shadow
    await ContentTask.spawn(browser, shouldBeDefined, async function (shouldBeDefined) {
      let iframe = content.document.querySelector("iframe");
      let rawIframe = ChromeUtils.waiveXrays(iframe);

      ok(rawIframe.contentWindow, "Not null");

      let messageEvent = await new Promise(resolve => {
        content.addEventListener("message",
                                 messageEvent => resolve(messageEvent),
                                 {once: true});
        rawIframe.contentWindow.postMessage("syn", "*");
      });

      ok(messageEvent, "message received");
      is(messageEvent.data, "ack", "ack received");

      let innerWindow = ChromeUtils.unwaiveXrays(rawIframe.contentWindow);

      is(content.frames.length > 0, shouldBeDefined, "Window.frames.length");
      is(content.frames[0] !== undefined, shouldBeDefined, "Window.frames - IndexedGetter");

      // Ignored test case:
      // - privileged -> unprivileged named getter is blocked by default.
      // is(content.frames["testframe"] !== undefined, shouldBeDefined, "Window.frames - Named Getter");

      is(iframe.contentWindow !== undefined, shouldBeDefined, "HTMLIframeElement.contentWindow");
      is(iframe.contentDocument !== undefined, shouldBeDefined, "HTMLIframeElement.contentDocument");

      is(innerWindow.parent !== undefined, shouldBeDefined, "Window.parent");
      is(innerWindow.top !== undefined, shouldBeDefined, "Window.top");
      is(innerWindow.opener !== undefined, shouldBeDefined, "Window.opener");

      is(messageEvent.source !== undefined, shouldBeDefined, "MessageEvent.source");
    });
  });
}
