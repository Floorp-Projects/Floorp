/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.events.testing.asyncClipboard", true]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:logins" },
    async function (browser) {
      let TEST_LOGIN = {
        guid: "70a",
        username: "jared",
        password: "deraj",
        origin: "https://www.example.com",
      };

      await SpecialPowers.spawn(browser, [TEST_LOGIN], async function (login) {
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

      let testCases = [[TEST_LOGIN.username, "copy-username-button"]];
      if (OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
        testCases[1] = [TEST_LOGIN.password, "copy-password-button"];
      }

      for (let testCase of testCases) {
        let testObj = {
          expectedValue: testCase[0],
          copyButtonSelector: testCase[1],
        };
        info(
          "waiting for " + testObj.expectedValue + " to be placed on clipboard"
        );
        let reauthObserved = true;
        if (testObj.copyButtonSelector.includes("password")) {
          reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
        }

        await SimpleTest.promiseClipboardChange(
          testObj.expectedValue,
          async () => {
            await SpecialPowers.spawn(
              browser,
              [testObj],
              async function (aTestObj) {
                let loginItem = content.document.querySelector("login-item");
                let copyButton = loginItem.shadowRoot.querySelector(
                  aTestObj.copyButtonSelector
                );
                info("Clicking 'copy' button");
                copyButton.click();
              }
            );
          }
        );
        await reauthObserved;
        Assert.ok(true, testObj.expectedValue + " is on clipboard now");

        await SpecialPowers.spawn(
          browser,
          [testObj],
          async function (aTestObj) {
            let loginItem = Cu.waiveXrays(
              content.document.querySelector("login-item")
            );
            let copyButton = loginItem.shadowRoot.querySelector(
              aTestObj.copyButtonSelector
            );
            let otherCopyButton =
              copyButton == loginItem._copyUsernameButton
                ? loginItem._copyPasswordButton
                : loginItem._copyUsernameButton;
            Assert.ok(
              !otherCopyButton.dataset.copied,
              "The other copy button should have the 'copied' state removed"
            );
            Assert.ok(
              copyButton.dataset.copied,
              "Success message should be shown"
            );
          }
        );
      }

      // Wait for the 'copied' attribute to get removed from the copyPassword
      // button, which is the last button that is clicked in the above testcase.
      // Since another Copy button isn't clicked, the state won't get cleared
      // instantly. This test covers the built-in timeout of the visual display.
      await SpecialPowers.spawn(browser, [], async () => {
        let copyButton = Cu.waiveXrays(
          content.document.querySelector("login-item")
        )._copyPasswordButton;
        await ContentTaskUtils.waitForCondition(
          () => !copyButton.dataset.copied,
          "'copied' attribute should be removed after a timeout"
        );
      });
    }
  );
});
