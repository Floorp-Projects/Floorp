/**
 * Tests of MasterPassword.jsm
 */

"use strict";

let {MasterPassword} = Cu.import("resource://formautofill/MasterPassword.jsm", {});

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

add_task(async function test_without_init_crypto() {
  MasterPassword._token.reset();
  Assert.equal(MasterPassword._token.needsUserInit, true);
});

TESTCASES.forEach(testcase => {
  let token = MasterPassword._token;

  add_task(async function test_encrypt_decrypt() {
    do_print("Starting testcase: " + testcase.description);
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
      token.logoutSimple();
      ok(!token.isLoggedIn(),
         "Token should be logged out after calling logoutSimple()");
      try {
        await MasterPassword.encrypt(testText);
        throw new Error("Not receiving canceled master password error");
      } catch (e) {
        Assert.equal(e.message, "User canceled master password entry");
      }
      try {
        await MasterPassword.decrypt(cipherText);
        throw new Error("Not receiving canceled master password error");
      } catch (e) {
        Assert.equal(e.message, "User canceled master password entry");
      }
    }

    token.reset();
  });
});
