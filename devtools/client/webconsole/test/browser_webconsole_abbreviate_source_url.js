/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that source URLs are abbreviated properly for display on the right-
// hand side of the Web Console.

"use strict";

function test() {
  testAbbreviation("http://example.com/x.js", "x.js");
  testAbbreviation("http://example.com/foo/bar/baz/boo.js", "boo.js");
  testAbbreviation("http://example.com/foo/bar/", "bar");
  testAbbreviation("http://example.com/foo.js?bar=1&baz=2", "foo.js");
  testAbbreviation("http://example.com/foo/?bar=1&baz=2", "foo");

  finishTest();
}

function testAbbreviation(aFullURL, aAbbreviatedURL) {
  is(WebConsoleUtils.abbreviateSourceURL(aFullURL), aAbbreviatedURL, aFullURL +
     " is abbreviated to " + aAbbreviatedURL);
}

