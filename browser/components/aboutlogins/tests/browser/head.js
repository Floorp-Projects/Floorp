/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);
let TEST_LOGIN1 = new nsLoginInfo(
  "https://example.com/",
  "https://example.com/",
  null,
  "user1",
  "pass1",
  "username",
  "password"
);
let TEST_LOGIN2 = new nsLoginInfo(
  "https://2.example.com/",
  "https://2.example.com/",
  null,
  "user2",
  "pass2",
  "username",
  "password"
);

async function addLogin(login) {
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  login = Services.logins.addLogin(login);
  await storageChangedPromised;
  registerCleanupFunction(() => {
    let matchData = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
    matchData.setPropertyAsAUTF8String("guid", login.guid);
    if (!Services.logins.searchLogins(matchData).length) {
      return;
    }
    Services.logins.removeLogin(login);
  });
  return login;
}
