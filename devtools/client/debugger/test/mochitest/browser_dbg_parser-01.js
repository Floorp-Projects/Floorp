/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that simple JS can be parsed and cached with the reflection API.
 */

function test() {
  let { Parser } = Cu.import("resource://devtools/shared/Parser.jsm", {});

  let source = "let x = 42;";
  let parser = new Parser();
  let first = parser.get(source);
  let second = parser.get(source);

  isnot(first, second,
    "The two syntax trees should be different.");

  let third = parser.get(source, "url");
  let fourth = parser.get(source, "url");

  isnot(first, third,
    "The new syntax trees should be different than the old ones.");
  is(third, fourth,
    "The new syntax trees were cached once an identifier was specified.");

  is(parser.errors.length, 0,
    "There should be no errors logged when parsing.");

  finish();
}
