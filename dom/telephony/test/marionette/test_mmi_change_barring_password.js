/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  // Test passing no password.
  {
    password: "",
    newPassword: "0000",
    newPasswordAgain: "1111",
    expectedError: {
      name: "emMmiErrorInvalidPassword"
    }
  },
  // Test passing no newPassword.
  {
    password: "0000",
    newPassword: "",
    newPasswordAgain: "",
    expectedError: {
      name: "emMmiErrorInvalidPassword"
    }
  },
  // Test passing mismatched newPassword.
  {
    password: "0000",
    newPassword: "0000",
    newPasswordAgain: "1111",
    expectedError: {
      name: "emMmiErrorMismatchPassword"
    }
  },
  // Test passing invalid password (not 4 digits).
  {
    password: "000",
    newPassword: "0000",
    newPasswordAgain: "0000",
    expectedError: {
      name: "emMmiErrorInvalidPassword"
    }
  },
  // Expect to succeed.
  {
    password: "0000",  // 0000 is the default password for call barring
    newPassword: "1234",
    newPasswordAgain: "1234"
  },
  // Restore to the default password
  {
    password: "1234",
    newPassword: "0000",
    newPasswordAgain: "0000"
  }
];

let MMI_PREFIX = [
  "*03*330*",
  "**03*330*",
  "*03**",
  "**03**",
];

function testChangeCallBarringPassword(aMMIPrefix, aPassword, aNewPassword,
                                       aNewPasswordAgain, aExpectedError) {
  let MMI_CODE = aMMIPrefix + aPassword + "*" + aNewPassword + "*" + aNewPasswordAgain + "#";
  log("Test " + MMI_CODE);

  return gSendMMI(MMI_CODE).then(aResult => {
    is(aResult.success, !aExpectedError, "check success");
    is(aResult.serviceCode, "scChangePassword", "Check service code");

    if (aResult.success) {
      is(aResult.statusMessage, "smPasswordChanged", "Check status message");
    } else {
      is(aResult.statusMessage, aExpectedError.name, "Check name");
    }
  });
}

// Start test
startTest(function() {
  let promise = Promise.resolve();

  for (let prefix of MMI_PREFIX) {
    for (let i = 0; i < TEST_DATA.length; i++) {
      let data = TEST_DATA[i];
      promise = promise.then(() => testChangeCallBarringPassword(prefix,
                                                                 data.password,
                                                                 data.newPassword,
                                                                 data.newPasswordAgain,
                                                                 data.expectedError));
    }
  }

  return promise
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
