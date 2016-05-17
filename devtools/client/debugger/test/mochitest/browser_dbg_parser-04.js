/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that faulty JS inside HTML can be separated and identified correctly.
 */

function test() {
  let { Parser } = Cu.import("resource://devtools/shared/Parser.jsm", {});

  let source = [
    "<!doctype html>",
    "<head>",
    "<SCRIPT>",
    "let a + 42;",
    "</SCRIPT>",
    "<script type='text/javascript'>",
    "let b = 42;",
    "</SCRIPT>",
    "<script type='text/javascript;version=1.8'>",
    "let c + 42;",
    "</SCRIPT>",
    "</head>"
  ].join("\n");
  let parser = new Parser();
  // Don't pollute the logs with exceptions that we are going to check anyhow.
  parser.logExceptions = false;
  let parsed = parser.get(source);

  ok(parsed,
    "HTML code should be parsed correctly.");
  is(parser.errors.length, 2,
    "There should be two errors logged when parsing.");

  is(parser.errors[0].name, "SyntaxError",
    "The correct first exception was caught.");
  is(parser.errors[0].message, "missing ; before statement",
    "The correct first exception was caught.");

  is(parser.errors[1].name, "SyntaxError",
    "The correct second exception was caught.");
  is(parser.errors[1].message, "missing ; before statement",
    "The correct second exception was caught.");

  is(parsed.scriptCount, 1,
    "There should be 1 script parsed in the parent HTML source.");

  is(parsed.getScriptInfo(source.indexOf("let a")).toSource(), "({start:-1, length:-1, index:-1})",
    "The first script shouldn't be considered valid.");
  is(parsed.getScriptInfo(source.indexOf("let b")).toSource(), "({start:85, length:13, index:0})",
    "The second script was located correctly.");
  is(parsed.getScriptInfo(source.indexOf("let c")).toSource(), "({start:-1, length:-1, index:-1})",
    "The third script shouldn't be considered valid.");

  finish();
}
