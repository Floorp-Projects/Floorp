/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const base64 = require("sdk/base64");

const text = "Awesome!";
const b64text = "QXdlc29tZSE=";

const utf8text = "\u2713 à la mode";
const badutf8text = "\u0013 à la mode";
const b64utf8text = "4pyTIMOgIGxhIG1vZGU=";

// 1 MB string
const longtext = 'fff'.repeat(333333);
const b64longtext = 'ZmZm'.repeat(333333);

exports["test base64.encode"] = function (assert) {
  assert.equal(base64.encode(text), b64text, "encode correctly")
}

exports["test base64.decode"] = function (assert) {
  assert.equal(base64.decode(b64text), text, "decode correctly")
}

exports["test base64.encode Unicode"] = function (assert) {

  assert.equal(base64.encode(utf8text, "utf-8"), b64utf8text,
    "encode correctly Unicode strings.")
}

exports["test base64.decode Unicode"] = function (assert) {

  assert.equal(base64.decode(b64utf8text, "utf-8"), utf8text,
    "decode correctly Unicode strings.")
}

exports["test base64.encode long string"] = function (assert) {

  assert.equal(base64.encode(longtext), b64longtext, "encode long strings")
}

exports["test base64.decode long string"] = function (assert) {

  assert.equal(base64.decode(b64longtext), longtext, "decode long strings")
}

exports["test base64.encode treats input as octet string"] = function (assert) {

  assert.equal(base64.encode("\u0066"), "Zg==",
    "treat octet string as octet string")
  assert.equal(base64.encode("\u0166"), "Zg==",
    "treat non-octet string as octet string")
  assert.equal(base64.encode("\uff66"), "Zg==",
    "encode non-octet string as octet string")
}

exports["test base64.encode with wrong charset"] = function (assert) {

  assert.throws(function() {
    base64.encode(utf8text, "utf-16");
  }, "The charset argument can be only 'utf-8'");

  assert.throws(function() {
    base64.encode(utf8text, "");
  }, "The charset argument can be only 'utf-8'");

  assert.throws(function() {
    base64.encode(utf8text, 8);
  }, "The charset argument can be only 'utf-8'");

}

exports["test base64.decode with wrong charset"] = function (assert) {

  assert.throws(function() {
    base64.decode(utf8text, "utf-16");
  }, "The charset argument can be only 'utf-8'");

  assert.throws(function() {
    base64.decode(utf8text, "");
  }, "The charset argument can be only 'utf-8'");

  assert.throws(function() {
    base64.decode(utf8text, 8);
  }, "The charset argument can be only 'utf-8'");

}

exports["test encode/decode Unicode without utf-8 as charset"] = function (assert) {

  assert.equal(base64.decode(base64.encode(utf8text)), badutf8text,
    "Unicode strings needs 'utf-8' charset or will be mangled"
  );

}

require("test").run(exports);
