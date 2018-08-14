/**
 * Tests of MasterPassword.jsm
 */

"use strict";
const {MockRegistrar} =
  ChromeUtils.import("resource://testing-common/MockRegistrar.jsm", {});
let {MasterPassword} = ChromeUtils.import("resource://formautofill/MasterPassword.jsm", {});

const TESTCASES = [{
  description: "With master password set",
  masterPassword: "fakemp",
  mpEnabled: true,
},
{
  description: "Without master password set",
  masterPassword: "", // "" means no master password
  mpEnabled: false,
}];


// Tests that PSM can successfully ask for a password from the user and relay it
// back to NSS. Does so by mocking out the actual dialog and "filling in" the
// password. Also tests that providing an incorrect password will fail (well,
// technically the user will just get prompted again, but if they then cancel
// the dialog the overall operation will fail).

let gMockPrompter = {
  passwordToTry: null,
  numPrompts: 0,

  // This intentionally does not use arrow function syntax to avoid an issue
  // where in the context of the arrow function, |this != gMockPrompter| due to
  // how objects get wrapped when going across xpcom boundaries.
  promptPassword(dialogTitle, text, password, checkMsg, checkValue) {
    this.numPrompts++;
    if (this.numPrompts > 1) { // don't keep retrying a bad password
      return false;
    }
    equal(text,
          "Please enter your master password.",
          "password prompt text should be as expected");
    equal(checkMsg, null, "checkMsg should be null");
    ok(this.passwordToTry, "passwordToTry should be non-null");
    password.value = this.passwordToTry;
    return true;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
};

// Mock nsIWindowWatcher. PSM calls getNewPrompter on this to get an nsIPrompt
// to call promptPassword. We return the mock one, above.
let gWindowWatcher = {
  getNewPrompter: () => gMockPrompter,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowWatcher]),
};

// Ensure that the appropriate initialization has happened.
do_get_profile();

let windowWatcherCID =
  MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1",
                         gWindowWatcher);
registerCleanupFunction(() => {
  MockRegistrar.unregister(windowWatcherCID);
});

TESTCASES.forEach(testcase => {
  let token = MasterPassword._token;

  add_task(async function test_encrypt_decrypt() {
    info("Starting testcase: " + testcase.description);
    token.initPassword(testcase.masterPassword);

    // Test only: Force the token login without asking for master password
    token.login(/* force */ false);
    Assert.equal(testcase.mpEnabled, token.isLoggedIn(), "Token should now be logged into");
    Assert.equal(MasterPassword.isEnabled, testcase.mpEnabled);

    let testText = "test string";
    let cipherText = await MasterPassword.encrypt(testText);
    Assert.notEqual(testText, cipherText);
    let plainText = await MasterPassword.decrypt(cipherText);
    Assert.equal(testText, plainText);
    if (token.isLoggedIn()) {
      // Reset state.
      gMockPrompter.numPrompts = 0;
      token.logoutSimple();

      ok(!token.isLoggedIn(),
         "Token should be logged out after calling logoutSimple()");

      // Try with the correct password.
      gMockPrompter.passwordToTry = testcase.masterPassword;
      await MasterPassword.encrypt(testText);
      Assert.equal(gMockPrompter.numPrompts, 1, "should have prompted for encryption");

      // Reset state.
      gMockPrompter.numPrompts = 0;
      token.logoutSimple();

      try {
        // Try with the incorrect password.
        gMockPrompter.passwordToTry = "XXX";
        await MasterPassword.decrypt(cipherText);
        throw new Error("Not receiving canceled master password error");
      } catch (e) {
        Assert.equal(e.message, "User canceled master password entry");
      }
    }

    token.reset();
  });
});
