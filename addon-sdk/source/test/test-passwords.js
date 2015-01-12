/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { store, search, remove } = require("sdk/passwords");

exports["test store requires `password` field"] = function(assert, done) {
  store({
    username: "foo",
    realm: "bar",
    onComplete: function onComplete() {
      assert.fail("onComplete should not be called");
    },
    onError: function onError() {
      assert.pass("'`password` is required");
      done();
    }
  });
};

exports["test store requires `username` field"] = function(assert, done) {
  store({
    password: "foo",
    realm: "bar",
    onComplete: function onComplete() {
      assert.fail("onComplete should not be called");
    },
    onError: function onError() {
      assert.pass("'`username` is required");
      done();
    }
  });
};

exports["test onComplete is optional"] = function(assert, done) {
  store({
    realm: "bla",
    username: "bla",
    password: "bla",
    onError: function onError() {
      assert.fail("onError was called");
    }
  });
  assert.pass("exception is not thrown if `onComplete is missing")
  done();
};

exports["test exceptions in onComplete are reported"] = function(assert, done) {
  store({
    realm: "throws",
    username: "error",
    password: "boom!",
    onComplete: function onComplete(error) {
      throw new Error("Boom!")
    },
    onError: function onError(error) {
      assert.equal(error.message, "Boom!", "Error thrown is reported");
      done();
    }
  });
};

exports["test store requires `realm` field"] = function(assert, done) {
  store({
    username: "foo",
    password: "bar",
    onComplete: function onComplete() {
      assert.fail("onComplete should not be called");
    },
    onError: function onError() {
      assert.pass("'`realm` is required");
      done();
    }
  });
};

exports["test can't store same login twice"] = function(assert, done) {
  store({
    username: "user",
    password: "pass",
    realm: "realm",
    onComplete: function onComplete() {
      assert.pass("credential saved");

      store({
        username: "user",
        password: "pass",
        realm: "realm",
        onComplete: function onComplete() {
          assert.fail("onComplete should not be called");
        },
        onError: function onError() {
          assert.pass("re-saving credential failed");

          remove({
            username: "user",
            password: "pass",
            realm: "realm",
            onComplete: function onComplete() {
              assert.pass("credential was removed");
              done();
            },
            onError: function onError() {
              assert.fail("remove should not fail");
            }
          });
        }
      });
    },
    onError: function onError() {
      assert.fail("onError should not be called");
    }
  });
};

exports["test remove fails if no login found"] = function(assert, done) {
  remove({
    username: "foo",
    password: "bar",
    realm: "baz",
    onComplete: function onComplete() {
      assert.fail("should not be able to remove unstored credentials");
    },
    onError: function onError() {
      assert.pass("can't remove unstored credentials");
      done();
    }
  });
};

exports["test addon associated credentials"] = function(assert, done) {
  store({
    username: "foo",
    password: "bar",
    realm: "baz",
    onComplete: function onComplete() {
      search({
        username: "foo",
        password: "bar",
        realm: "baz",
        onComplete: function onComplete([credential]) {
          assert.equal(credential.url.indexOf("addon:"), 0,
                       "`addon:` uri is used for add-on credentials");
          assert.equal(credential.username, "foo",
                       "username matches");
          assert.equal(credential.password, "bar",
                       "password matches");
          assert.equal(credential.realm, "baz", "realm matches");
          assert.equal(credential.formSubmitURL, null,
                       "`formSubmitURL` is `null` for add-on credentials");
          assert.equal(credential.usernameField, "", "usernameField is empty");
          assert.equal(credential.passwordField, "", "passwordField is empty");

          remove({
            username: credential.username,
            password: credential.password,
            realm: credential.realm,
            onComplete: function onComplete() {
              assert.pass("credential is removed");
              done();
            },
            onError: function onError() {
              assert.fail("onError should not be called");
            }
          });
        },
        onError: function onError() {
          assert.fail("onError should not be called");
        }
      });
    },
    onError: function onError() {
      assert.fail("onError should not be called");
    }
  });
};

exports["test web page associated credentials"] = function(assert, done) {
  store({
    url: "http://bar.foo.com/authentication/?login",
    formSubmitURL: "http://login.foo.com/authenticate.cgi",
    username: "user",
    password: "pass",
    usernameField: "user-f",
    passwordField: "pass-f",
    onComplete: function onComplete() {
      search({
        username: "user",
        password: "pass",
        url: "http://bar.foo.com",
        formSubmitURL: "http://login.foo.com",
        onComplete: function onComplete([credential]) {
          assert.equal(credential.url, "http://bar.foo.com", "url matches");
          assert.equal(credential.username, "user", "username matches");
          assert.equal(credential.password, "pass", "password matches");
          assert.equal(credential.realm, null, "realm is null");
          assert.equal(credential.formSubmitURL, "http://login.foo.com",
                       "formSubmitURL matches");
          assert.equal(credential.usernameField, "user-f",
                       "usernameField is matches");
          assert.equal(credential.passwordField, "pass-f",
                       "passwordField matches");

          remove({
            url: credential.url,
            formSubmitURL: credential.formSubmitURL,
            username: credential.username,
            password: credential.password,
            usernameField: credential.usernameField,
            passwordField: credential.passwordField,

            onComplete: function onComplete() {
              assert.pass("credential is removed");
              done();
            },
            onError: function onError(e) {
              assert.fail("onError should not be called");
            }
          });
        },
        onError: function onError() {
          assert.fail("onError should not be called");
        }
      });
    },
    onError: function onError() {
      assert.fail("onError should not be called");
    }
  });
};

exports["test site authentication credentials"] = function(assert, done) {
  store({
    url: "http://authentication.com",
    username: "U",
    password: "P",
    realm: "R",
    onComplete: function onComplete() {
      search({
        url: "http://authentication.com",
        username: "U",
        password: "P",
        realm: "R",
        onComplete: function onComplete([credential]) {
          assert.equal(credential.url,"http://authentication.com",
                       "url matches");
          assert.equal(credential.username, "U", "username matches");
          assert.equal(credential.password, "P", "password matches");
          assert.equal(credential.realm, "R", "realm matches");
          assert.equal(credential.formSubmitURL, null, "formSubmitURL is null");
          assert.equal(credential.usernameField, "", "usernameField is empty");
          assert.equal(credential.passwordField, "", "passwordField is empty");

          remove({
            url: credential.url,
            username: credential.username,
            password: credential.password,
            realm: credential.realm,
            onComplete: function onComplete() {
              assert.pass("credential is removed");
              done();
            },
            onError: function onError() {
              assert.fail("onError should not be called");
            }
          });
        },
        onError: function onError() {
          assert.fail("onError should not be called");
        }
      });
    },
    onError: function onError() {
      assert.fail("onError should not be called");
    }
  });
};

require("test").run(exports);
