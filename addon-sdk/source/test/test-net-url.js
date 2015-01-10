/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { readURI, readURISync } = require("sdk/net/url");
const data = require("./fixtures");

const utf8text = "Hello, ゼロ!";
const latin1text = "Hello, ã‚¼ãƒ­!";

const dataURIutf8 = "data:text/plain;charset=utf-8," + encodeURIComponent(utf8text);
const dataURIlatin1 = "data:text/plain;charset=ISO-8859-1," + escape(latin1text);
const chromeURI = "chrome://global-platform/locale/accessible.properties";

exports["test async readURI"] = function(assert, done) {
  let content = "";

  readURI(data.url("test-net-url.txt")).then(function(data) {
    content = data;
    assert.equal(content, utf8text, "The URL content is loaded properly");
    done();
  }, function() {
    assert.fail("should not reject");
    done();
  })

  assert.equal(content, "", "The URL content is not load yet");
}

exports["test readURISync"] = function(assert) {
  let content = readURISync(data.url("test-net-url.txt"));

  assert.equal(content, utf8text, "The URL content is loaded properly");
}

exports["test async readURI with ISO-8859-1 charset"] = function(assert, done) {
  let content = "";

  readURI(data.url("test-net-url.txt"), { charset : "ISO-8859-1"}).then(function(data) {
    content = data;
    assert.equal(content, latin1text, "The URL content is loaded properly");
    done();
  }, function() {
    assert.fail("should not reject");
    done();
  })

  assert.equal(content, "", "The URL content is not load yet");
}

exports["test readURISync with ISO-8859-1 charset"] = function(assert) {
  let content = readURISync(data.url("test-net-url.txt"), "ISO-8859-1");

  assert.equal(content, latin1text, "The URL content is loaded properly");
}

exports["test async readURI with not existing file"] = function(assert, done) {
  readURI(data.url("test-net-url-fake.txt")).then(function(data) {
    assert.fail("should not resolve");
    done();
  }, function(reason) {
    assert.ok(reason.indexOf("Failed to read:") === 0);
    done();
  })
}

exports["test readURISync with not existing file"] = function(assert) {
  assert.throws(function() {
    readURISync(data.url("test-net-url-fake.txt"));
  }, /NS_ERROR_FILE_NOT_FOUND/);
}

exports["test async readURI with data URI"] = function(assert, done) {
  let content = "";

  readURI(dataURIutf8).then(function(data) {
    content = data;
    assert.equal(content, utf8text, "The URL content is loaded properly");
    done();
  }, function() {
    assert.fail("should not reject");
    done();
  })

  assert.equal(content, "", "The URL content is not load yet");
}

exports["test readURISync with data URI"] = function(assert) {
  let content = readURISync(dataURIutf8);

  assert.equal(content, utf8text, "The URL content is loaded properly");
}

exports["test async readURI with data URI and ISO-8859-1 charset"] = function(assert, done) {
  let content = "";

  readURI(dataURIlatin1, { charset : "ISO-8859-1"}).then(function(data) {
    content = unescape(data);
    assert.equal(content, latin1text, "The URL content is loaded properly");
    done();
  }, function() {
    assert.fail("should not reject");
    done();
  })

  assert.equal(content, "", "The URL content is not load yet");
}

exports["test readURISync with data URI and ISO-8859-1 charset"] = function(assert) {
  let content = unescape(readURISync(dataURIlatin1, "ISO-8859-1"));

  assert.equal(content, latin1text, "The URL content is loaded properly");
}

exports["test readURISync with chrome URI"] = function(assert) {
  let content = readURISync(chromeURI);

  assert.ok(content, "The URL content is loaded properly");
}

exports["test async readURI with chrome URI"] = function(assert, done) {
  let content = "";

  readURI(chromeURI).then(function(data) {
    content = data;
    assert.equal(content, readURISync(chromeURI), "The URL content is loaded properly");
    done();
  }, function() {
    assert.fail("should not reject");
    done();
  })

  assert.equal(content, "", "The URL content is not load yet");
}

require("sdk/test").run(exports)
