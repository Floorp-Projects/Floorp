/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { MatchPattern } = require("sdk/util/match-pattern");

exports.testMatchPatternTestTrue = function(assert) {
  function ok(pattern, url) {
    let mp = new MatchPattern(pattern);
    assert.ok(mp.test(url), pattern + " should match " + url);
  }

  ok("*", "http://example.com");
  ok("*", "https://example.com");
  ok("*", "ftp://example.com");

  ok("*.example.com", "http://example.com");
  ok("*.example.com", "http://hamburger.example.com");
  ok("*.example.com", "http://hotdog.hamburger.example.com");

  ok("http://example.com*", "http://example.com");
  ok("http://example.com*", "http://example.com/");
  ok("http://example.com/*", "http://example.com/");
  ok("http://example.com/*", "http://example.com/potato-salad");
  ok("http://example.com/pickles/*", "http://example.com/pickles/");
  ok("http://example.com/pickles/*", "http://example.com/pickles/lemonade");

  ok("http://example.com", "http://example.com");
  ok("http://example.com/ice-cream", "http://example.com/ice-cream");

  ok(/.*zilla.*/, "https://bugzilla.redhat.com/show_bug.cgi?id=569753");
  ok(/https:.*zilla.*/, "https://bugzilla.redhat.com/show_bug.cgi?id=569753");
  ok('*.sample.com', 'http://ex.sample.com/foo.html');
  ok('*.amp.le.com', 'http://ex.amp.le.com');

  ok('data:*', 'data:text/html;charset=utf-8,');
};

exports.testMatchPatternTestFalse = function(assert) {
  function ok(pattern, url) {
    let mp = new MatchPattern(pattern);
    assert.ok(!mp.test(url), pattern + " should not match " + url);
  }

  ok("*", null);
  ok("*", "");
  ok("*", "bogus");
  ok("*", "chrome://browser/content/browser.xul");
  ok("*", "nttp://example.com");

  ok("*.example.com", null);
  ok("*.example.com", "");
  ok("*.example.com", "bogus");
  ok("*.example.com", "http://example.net");
  ok("*.example.com", "http://foo.com");
  ok("*.example.com", "http://example.com.foo");
  ok("*.example2.com", "http://example.com");

  ok("http://example.com/*", null);
  ok("http://example.com/*", "");
  ok("http://example.com/*", "bogus");
  ok("http://example.com/*", "http://example.com");
  ok("http://example.com/*", "http://foo.com/");

  ok("http://example.com", null);
  ok("http://example.com", "");
  ok("http://example.com", "bogus");
  ok("http://example.com", "http://example.com/");

  ok(/zilla.*/, "https://bugzilla.redhat.com/show_bug.cgi?id=569753");
  ok(/.*zilla/, "https://bugzilla.redhat.com/show_bug.cgi?id=569753");
  ok(/.*Zilla.*/, "https://bugzilla.redhat.com/show_bug.cgi?id=655464"); // bug 655464
  ok(/https:.*zilla/, "https://bugzilla.redhat.com/show_bug.cgi?id=569753");

  // bug 856913
  ok('*.ign.com', 'http://www.design.com');
  ok('*.ign.com', 'http://design.com');
  ok('*.zilla.com', 'http://bugzilla.mozilla.com');
  ok('*.zilla.com', 'http://mo-zilla.com');
  ok('*.amp.le.com', 'http://amp-le.com');
  ok('*.amp.le.com', 'http://examp.le.com');
};

exports.testMatchPatternErrors = function(assert) {
  assert.throws(
    function() new MatchPattern("*.google.com/*"),
    /There can be at most one/,
    "MatchPattern throws when supplied multiple '*'"
  );

  assert.throws(
    function() new MatchPattern("google.com"),
    /expected to be either an exact URL/,
    "MatchPattern throws when the wildcard doesn't use '*' and doesn't " +
    "look like a URL"
  );

  assert.throws(
    function() new MatchPattern("http://google*.com"),
    /expected to be the first or the last/,
    "MatchPattern throws when a '*' is in the middle of the wildcard"
  );

  assert.throws(
    function() new MatchPattern(/ /g),
    /^A RegExp match pattern cannot be set to `global` \(i\.e\. \/\/g\)\.$/,
    "MatchPattern throws on a RegExp set to `global` (i.e. //g)."
  );

  assert.throws(
    function() new MatchPattern(/ /i),
    /^A RegExp match pattern cannot be set to `ignoreCase` \(i\.e\. \/\/i\)\.$/,
    "MatchPattern throws on a RegExp set to `ignoreCase` (i.e. //i)."
  );

  assert.throws(
    function() new MatchPattern( / /m ),
    /^A RegExp match pattern cannot be set to `multiline` \(i\.e\. \/\/m\)\.$/,
    "MatchPattern throws on a RegExp set to `multiline` (i.e. //m)."
  );
};

exports.testMatchPatternInternals = function(assert) {
  assert.equal(
    new MatchPattern("http://google.com/test").exactURL,
    "http://google.com/test"
  );

  assert.equal(
    new MatchPattern("http://google.com/test/*").urlPrefix,
    "http://google.com/test/"
  );

  assert.equal(
    new MatchPattern("*.example.com").domain,
    "example.com"
  );
};

require('sdk/test').run(exports);
