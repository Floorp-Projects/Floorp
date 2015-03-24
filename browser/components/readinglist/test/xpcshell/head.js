/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

do_get_profile(); // fxa needs a profile directory for storage.

Cu.import("resource://gre/modules/FxAccounts.jsm");
Cu.import("resource://gre/modules/FxAccountsClient.jsm");

// Create a mocked FxAccounts object with a signed-in, verified user.
function* createMockFxA() {

  function MockFxAccountsClient() {
    this._email = "nobody@example.com";
    this._verified = false;

    this.accountStatus = function(uid) {
      let deferred = Promise.defer();
      deferred.resolve(!!uid && (!this._deletedOnServer));
      return deferred.promise;
    };

    this.signOut = function() { return Promise.resolve(); };

    FxAccountsClient.apply(this);
  }

  MockFxAccountsClient.prototype = {
    __proto__: FxAccountsClient.prototype
  }

  function MockFxAccounts() {
    return new FxAccounts({
      fxAccountsClient: new MockFxAccountsClient(),
      getAssertion: () => Promise.resolve("assertion"),
    });
  }

  let fxa = new MockFxAccounts();
  let credentials = {
    email: "foo@example.com",
    uid: "1234@lcip.org",
    assertion: "foobar",
    sessionToken: "dead",
    kA: "beef",
    kB: "cafe",
    verified: true
  };

  yield fxa.setSignedInUser(credentials);
  return fxa;
}
