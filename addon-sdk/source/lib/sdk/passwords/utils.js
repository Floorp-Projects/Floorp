/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci, CC } = require("chrome");
const { uri: ADDON_URI } = require("../self");
const loginManager = Cc["@mozilla.org/login-manager;1"].
                     getService(Ci.nsILoginManager);
const { URL: parseURL } = require("../url");
const LoginInfo = CC("@mozilla.org/login-manager/loginInfo;1",
                     "nsILoginInfo", "init");

function filterMatchingLogins(loginInfo)
  Object.keys(this).every(function(key) loginInfo[key] === this[key], this);

/**
 * Removes `user`, `password` and `path` fields from the given `url` if it's
 * 'http', 'https' or 'ftp'. All other URLs are returned unchanged.
 * @example
 * http://user:pass@www.site.com/foo/?bar=baz#bang -> http://www.site.com
 */
function normalizeURL(url) {
  let { scheme, host, port } = parseURL(url);
  // We normalize URL only if it's `http`, `https` or `ftp`. All other types of
  // URLs (`resource`, `chrome`, etc..) should not be normalized as they are
  // used with add-on associated credentials path.
  return scheme === "http" || scheme === "https" || scheme === "ftp" ?
         scheme + "://" + (host || "") + (port ? ":" + port : "") :
         url
}

function Login(options) {
  let login = Object.create(Login.prototype);
  Object.keys(options || {}).forEach(function(key) {
    if (key === 'url')
      login.hostname = normalizeURL(options.url);
    else if (key === 'formSubmitURL')
      login.formSubmitURL = options.formSubmitURL ?
                            normalizeURL(options.formSubmitURL) : null;
    else if (key === 'realm')
      login.httpRealm = options.realm;
    else 
      login[key] = options[key];
  });

  return login;
}
Login.prototype.toJSON = function toJSON() {
  return {
    url: this.hostname || ADDON_URI,
    realm: this.httpRealm || null,
    formSubmitURL: this.formSubmitURL || null,
    username: this.username || null,
    password: this.password || null,
    usernameField: this.usernameField || '',
    passwordField: this.passwordField || '',
  }
};
Login.prototype.toLoginInfo = function toLoginInfo() {
  let { url, realm, formSubmitURL, username, password, usernameField,
        passwordField } = this.toJSON();

  return new LoginInfo(url, formSubmitURL, realm, username, password,
                       usernameField, passwordField);
};

function loginToJSON(value) Login(value).toJSON()

/**
 * Returns array of `nsILoginInfo` objects that are stored in the login manager
 * and have all the properties with matching values as a given `options` object.
 * @param {Object} options
 * @returns {nsILoginInfo[]}
 */
exports.search = function search(options) {
  return loginManager.getAllLogins()
                     .filter(filterMatchingLogins, Login(options))
                     .map(loginToJSON);
};

/**
 * Stores login info created from the given `options` to the applications
 * built-in login management system.
 * @param {Object} options.
 */
exports.store = function store(options) {
  loginManager.addLogin(Login(options).toLoginInfo());
};

/**
 * Removes login info from the applications built-in login management system.
 * _Please note: When removing a login info the specified properties must
 * exactly match to the one that is already stored or exception will be thrown._
 * @param {Object} options.
 */
exports.remove = function remove(options) {
  loginManager.removeLogin(Login(options).toLoginInfo());
};
