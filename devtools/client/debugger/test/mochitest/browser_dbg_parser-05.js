/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that JS code containing strings that might look like <script> tags
 * inside an HTML source is parsed correctly.
 */

function test() {
  let { Parser } = Cu.import("resource://devtools/shared/Parser.jsm", {});

  let source = [
    "let a = [];",
    "a.push('<script>');",
    "a.push('var a = 42;');",
    "a.push('</script>');",
    "a.push('<script type=\"text/javascript\">');",
    "a.push('var b = 42;');",
    "a.push('</script>');",
    "a.push('<script type=\"text/javascript\">');",
    "a.push('var c = 42;');",
    "a.push('</script>');"
  ].join("\n");
  let parser = new Parser();
  let parsed = parser.get(source);

  ok(parsed,
    "The javascript code should be parsed correctly.");
  is(parser.errors.length, 0,
    "There should be no errors logged when parsing.");

  is(parsed.scriptCount, 1,
    "There should be 1 script parsed in the parent source.");

  is(parsed.getScriptInfo(source.indexOf("let a")).toSource(), "({start:0, length:249, index:0})",
    "The script location is correct (1).");
  is(parsed.getScriptInfo(source.indexOf("<script>")).toSource(), "({start:0, length:249, index:0})",
    "The script location is correct (2).");
  is(parsed.getScriptInfo(source.indexOf("</script>")).toSource(), "({start:0, length:249, index:0})",
    "The script location is correct (3).");

  finish();
}
