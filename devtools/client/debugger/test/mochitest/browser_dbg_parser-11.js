/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Checks if self-closing <script/> tags are parsed by Parser.jsm
 */

function test() {
  let { Parser } = Cu.import("resource://devtools/shared/Parser.jsm", {});

  let source = [
    '<script type="text/javascript" src="chrome://foo.js"/>',
    '<script type="application/javascript" src="chrome://baz.js"/>',
    '<script async defer src="chrome://foobar.js"/>',
    '<script type="application/javascript"/>"hello third"',
    '<script type="application/javascript">"hello fourth"</script>',
  ].join("\n");
  let parser = new Parser();
  let parsed = parser.get(source);

  is(parser.errors.length, 0,
    "There should be no errors logged when parsing.");
  is(parsed.scriptCount, 5,
    "There should be 5 scripts parsed in the parent HTML source.");

  is(parsed.getScriptInfo(source.indexOf("foo.js\"/>") + 1).toSource(), "({start:-1, length:-1, index:-1})",
    "the first script is empty");
  is(parsed.getScriptInfo(source.indexOf("baz.js\"/>") + 1).toSource(), "({start:-1, length:-1, index:-1})",
    "the second script is empty");
  is(parsed.getScriptInfo(source.indexOf("foobar.js\"/>") + 1).toSource(), "({start:-1, length:-1, index:-1})",
    "the third script is empty");

  is(parsed.getScriptInfo(source.indexOf("hello third!")).toSource(), "({start:-1, length:-1, index:-1})",
    "Inline script on self-closing tag not considered a script");
  is(parsed.getScriptInfo(source.indexOf("hello fourth")).toSource(), "({start:255, length:14, index:4})",
    "The fourth script was located correctly.");

  finish();
}
