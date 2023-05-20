/**
 * Test that the doorhanger notification for password saving is populated with
 * the correct values in various password capture cases.
 */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/passwordmgr/test/browser/head.js",
  this
);

add_task(async function test_policy_masterpassword_doorhanger() {
  await setupPolicyEngineWithJson({
    policies: {
      PrimaryPassword: true,
    },
  });

  let username = "username";
  let password = "password";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "https://example.com/browser/toolkit/components/" +
        "passwordmgr/test/browser/form_basic.html",
    },
    async function (browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);

      // Update the form with credentials from the test case.
      info(`update form with username: ${username}, password: ${password}`);
      await changeContentFormValues(browser, {
        "#form-basic-username": username,
        "#form-basic-password": password,
      });

      // Submit the form with the new credentials. This will cause the doorhanger
      // notification to be displayed.
      let formSubmittedPromise = listenForTestNotification("ShowDoorhanger");
      await SpecialPowers.spawn(browser, [], async function () {
        let doc = this.content.document;
        doc.getElementById("form-basic").submit();
      });
      await formSubmittedPromise;

      let expectedDoorhanger = "password-save";

      info("Waiting for doorhanger of type: " + expectedDoorhanger);
      let notif = await waitForDoorhanger(browser, expectedDoorhanger);

      // Fake the subdialog
      let dialogURL = "";
      let originalOpenDialog = window.openDialog;
      window.openDialog = function (aDialogURL, unused, unused2, aCallback) {
        dialogURL = aDialogURL;
        if (aCallback) {
          aCallback();
        }
      };

      await clickDoorhangerButton(notif, REMEMBER_BUTTON);

      await TestUtils.waitForCondition(
        () => dialogURL,
        "wait for open to get called asynchronously"
      );
      is(
        dialogURL,
        "chrome://mozapps/content/preferences/changemp.xhtml",
        "clicking on the checkbox should open the masterpassword dialog"
      );
      window.openDialog = originalOpenDialog;
    }
  );
});
