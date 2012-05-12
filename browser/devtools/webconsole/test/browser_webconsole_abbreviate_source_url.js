/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs are abbreviated properly for display on the right-
// hand side of the Web Console.

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

