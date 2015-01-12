/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { store, search, remove } = require("sdk/passwords/utils");

exports["test store requires `password` field"] = function(assert) {
  assert.throws(function() {
    store({ username: "foo", realm: "bar" });
  }, "`password` is required");
};

exports["test store requires `username` field"] = function(assert) {
  assert.throws(function() {
    store({ password: "foo", realm: "bar" });
  }, "`username` is required");
};

exports["test store requires `realm` field"] = function(assert) {
  assert.throws(function() {
    store({ username: "foo", password: "bar" });
  }, "`password` is required");
};

exports["test can't store same login twice"] = function(assert) {
  let options = { username: "user", password: "pass", realm: "realm" };
  store(options);
  assert.throws(function() {
    store(options);
  }, "can't store same pass twice");
  remove(options);
};

exports["test remove throws if no login found"] = function(assert) {
  assert.throws(function() {
    remove({ username: "foo", password: "bar", realm: "baz" });
  }, "can't remove unstored credentials");
};

exports["test addon associated credentials"] = function(assert) {
  let options = { username: "foo", password: "bar", realm: "baz" };
  store(options);

  assert.ok(search().length, "credential was stored");
  assert.ok(search(options).length, "stored credential found");
  assert.ok(search({ username: options.username }).length, "found by username");
  assert.ok(search({ password: options.password }).length, "found by password");
  assert.ok(search({ realm: options.realm }).length, "found by realm");

  let credential = search(options)[0];
  assert.equal(credential.url.indexOf("addon:"), 0,
               "`addon:` uri is used for add-on associated credentials");
  assert.equal(credential.username, options.username, "username matches");
  assert.equal(credential.password, options.password, "password matches");
  assert.equal(credential.realm, options.realm, "realm matches");
  assert.equal(credential.formSubmitURL, null,
               "`formSubmitURL` is `null` for add-on associated credentials");
  assert.equal(credential.usernameField, "", "usernameField is empty");
  assert.equal(credential.passwordField, "", "passwordField is empty");

  remove(search(options)[0]);
  assert.ok(!search(options).length, "remove worked");
};

exports["test web page associated credentials"] = function(assert) {
  let options = {
    url: "http://www.example.com",
    formSubmitURL: "http://login.example.com",
    username: "user",
    password: "pass",
    usernameField: "user-f",
    passwordField: "pass-f"
  };
  store({
    url: "http://www.example.com/login",
    formSubmitURL: "http://login.example.com/foo/authenticate.cgi",
    username: options.username,
    password: options.password,
    usernameField: options.usernameField,
    passwordField: options.passwordField
  });

  assert.ok(search().length, "credential was stored");
  assert.ok(search(options).length, "stored credential found");
  assert.ok(search({ username: options.username }).length, "found by username");
  assert.ok(search({ password: options.password }).length, "found by password");
  assert.ok(search({ formSubmitURL: options.formSubmitURL }).length,
            "found by formSubmitURL");
  assert.ok(search({ usernameField: options.usernameField }).length,
            "found by usernameField");
  assert.ok(search({ passwordField: options.passwordField }).length,
            "found by passwordField");

  let credential = search(options)[0];
  assert.equal(credential.url, options.url, "url matches");
  assert.equal(credential.username, options.username, "username matches");
  assert.equal(credential.password, options.password, "password matches");
  assert.equal(credential.realm, null, "realm is ");
  assert.equal(credential.formSubmitURL, options.formSubmitURL,
               "`formSubmitURL` matches");
  assert.equal(credential.usernameField, options.usernameField,
               "usernameField matches");
  assert.equal(credential.passwordField, options.passwordField,
               "passwordField matches");

  remove(search(options)[0]);
  assert.ok(!search(options).length, "remove worked");
};

exports["test site authentication credentials"] = function(assert) {
  let options = {
    url: "http://test.authentication.com",
    username: "u",
    password: "p",
    realm: "r"
  };

  store(options);
  assert.ok(search().length, "credential was stored");
  assert.ok(search(options).length, "stored credential found");
  assert.ok(search({ username: options.username }).length, "found by username");
  assert.ok(search({ password: options.password }).length, "found by password");
  assert.ok(search({ realm: options.realm }).length, "found by realm");
  assert.ok(search({ url: options.url }).length, "found by url");

  let credential = search(options)[0];
  assert.equal(credential.url, options.url, "url matches");
  assert.equal(credential.username, options.username, "username matches");
  assert.equal(credential.password, options.password, "password matches");
  assert.equal(credential.realm, options.realm, "realm matches");
  assert.equal(credential.formSubmitURL, null,
               "`formSubmitURL` is `null` for site authentication credentials");
  assert.equal(credential.usernameField, "", "usernameField is empty");
  assert.equal(credential.passwordField, "", "passwordField is empty");

  remove(search(options)[0]);
  assert.ok(!search(options).length, "remove worked");
};

require("test").run(exports);
