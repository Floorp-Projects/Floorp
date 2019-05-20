/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({"set": [["dom.events.testing.asyncClipboard", true]] });

  await BrowserTestUtils.withNewTab({gBrowser, url: "about:logins"}, async function(browser) {
    let TEST_LOGIN = {
      guid: "70a",
      username: "jared",
      password: "deraj",
      hostname: "https://www.example.com",
    };

    await ContentTask.spawn(browser, TEST_LOGIN, async function(login) {
      let loginItem = Cu.waiveXrays(content.document.querySelector("login-item"));

      // The login object needs to be cloned into the content global.
      loginItem.setLogin(Cu.cloneInto(login, content));

      // Lower the timeout for the test.
      let copyButton = loginItem.shadowRoot.querySelector(".copy-username-button");
      Object.defineProperty(copyButton.constructor, "BUTTON_RESET_TIMEOUT", {
          configurable: true,
          writable: true,
          value: 1000,
      });
    });

    for (let testCase of [
      [TEST_LOGIN.username, ".copy-username-button"],
      [TEST_LOGIN.password, ".copy-password-button"],
    ]) {
      let testObj = {
        expectedValue: testCase[0],
        copyButtonSelector: testCase[1],
      };
      await SimpleTest.promiseClipboardChange(testObj.expectedValue, async () => {
        await ContentTask.spawn(browser, testObj, async function(aTestObj) {
          let loginItem = content.document.querySelector("login-item");
          let copyButton = loginItem.shadowRoot.querySelector(aTestObj.copyButtonSelector);
          let innerButton = copyButton.shadowRoot.querySelector("button");
          info("Clicking 'copy' button");
          innerButton.click();
        });
      });
      ok(true, "Username is on clipboard now");

      await ContentTask.spawn(browser, testObj, async function(aTestObj) {
        let loginItem = content.document.querySelector("login-item");
        let copyButton = loginItem.shadowRoot.querySelector(aTestObj.copyButtonSelector);
        ok(copyButton.hasAttribute("copied"), "Success message should be shown");
        await ContentTaskUtils.waitForCondition(() => !copyButton.hasAttribute("copied"),
          "'copied' attribute should be removed after a timeout");
      });
    }
  });
});
