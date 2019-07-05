/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.events.testing.asyncClipboard", true]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async function(browser) {
      let TEST_LOGIN = {
        guid: "70a",
        username: "jared",
        password: "deraj",
        origin: "https://www.example.com",
      };

      await ContentTask.spawn(browser, TEST_LOGIN, async function(login) {
        let loginItem = Cu.waiveXrays(
          content.document.querySelector("login-item")
        );

        // The login object needs to be cloned into the content global.
        loginItem.setLogin(Cu.cloneInto(login, content));

        // Lower the timeout for the test.
        Object.defineProperty(
          loginItem.constructor,
          "COPY_BUTTON_RESET_TIMEOUT",
          {
            configurable: true,
            writable: true,
            value: 1000,
          }
        );
      });

      for (let testCase of [
        [TEST_LOGIN.username, ".copy-username-button"],
        [TEST_LOGIN.password, ".copy-password-button"],
      ]) {
        let testObj = {
          expectedValue: testCase[0],
          copyButtonSelector: testCase[1],
        };
        info(
          "waiting for " + testObj.expectedValue + " to be placed on clipboard"
        );
        await SimpleTest.promiseClipboardChange(
          testObj.expectedValue,
          async () => {
            await ContentTask.spawn(browser, testObj, async function(aTestObj) {
              let loginItem = content.document.querySelector("login-item");
              let copyButton = loginItem.shadowRoot.querySelector(
                aTestObj.copyButtonSelector
              );
              info("Clicking 'copy' button");
              copyButton.click();
            });
          }
        );
        ok(true, testObj.expectedValue + " is on clipboard now");

        await ContentTask.spawn(browser, testObj, async function(aTestObj) {
          let loginItem = content.document.querySelector("login-item");
          let copyButton = loginItem.shadowRoot.querySelector(
            aTestObj.copyButtonSelector
          );
          ok(copyButton.dataset.copied, "Success message should be shown");
          await ContentTaskUtils.waitForCondition(
            () => !copyButton.dataset.copied,
            "'copied' attribute should be removed after a timeout"
          );
        });
      }
    }
  );
});
