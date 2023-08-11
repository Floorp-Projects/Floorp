/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_PAGE =
  "https://example.com/browser/browser/base/content/test/permissions/empty.html";

add_task(async function testNoPermissionPrompt() {
  info("Creating tab");
  await BrowserTestUtils.withNewTab(TEST_PAGE, async function (browser) {
    await new Promise(r => {
      SpecialPowers.pushPrefEnv(
        {
          set: [
            ["dom.vibrator.enabled", true],
            ["dom.security.featurePolicy.header.enabled", true],
            ["dom.security.featurePolicy.webidl.enabled", true],
          ],
        },
        r
      );
    });

    await ContentTask.spawn(browser, null, async function () {
      let frame = content.document.createElement("iframe");
      // Cross origin src
      frame.src =
        "https://example.org/browser/browser/base/content/test/permissions/empty.html";
      await new Promise(resolve => {
        frame.addEventListener("load", () => {
          resolve();
        });
        content.document.body.appendChild(frame);
      });

      await content.SpecialPowers.spawn(frame, [], async function () {
        // Request a permission.
        let result = this.content.navigator.vibrate([100, 100]);
        Assert.equal(result, false, "navigator.vibrate has been denied");
      });
      content.document.body.removeChild(frame);
    });
  });
});
