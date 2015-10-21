/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that JS inside HTML can be separated and parsed correctly.
 */

function test() {
  let { Parser } = Cu.import("resource://devtools/shared/Parser.jsm", {});

  let source = [
    "<!doctype html>",
    "<head>",
      "<script>",
        "let a = 42;",
      "</script>",
      "<script type='text/javascript'>",
        "let b = 42;",
      "</script>",
      "<script type='text/javascript;version=1.8'>",
        "let c = 42;",
      "</script>",
    "</head>"
  ].join("\n");
  let parser = new Parser();
  let parsed = parser.get(source);

  ok(parsed,
    "HTML code should be parsed correctly.");
  is(parser.errors.length, 0,
    "There should be no errors logged when parsing.");

  is(parsed.scriptCount, 3,
    "There should be 3 scripts parsed in the parent HTML source.");

  is(parsed.getScriptInfo(0).toSource(), "({start:-1, length:-1, index:-1})",
    "There is no script at the beginning of the parent source.");
  is(parsed.getScriptInfo(source.length - 1).toSource(), "({start:-1, length:-1, index:-1})",
    "There is no script at the end of the parent source.");

  is(parsed.getScriptInfo(source.indexOf("let a")).toSource(), "({start:31, length:13, index:0})",
    "The first script was located correctly.");
  is(parsed.getScriptInfo(source.indexOf("let b")).toSource(), "({start:85, length:13, index:1})",
    "The second script was located correctly.");
  is(parsed.getScriptInfo(source.indexOf("let c")).toSource(), "({start:151, length:13, index:2})",
    "The third script was located correctly.");

  is(parsed.getScriptInfo(source.indexOf("let a") - 1).toSource(), "({start:31, length:13, index:0})",
    "The left edge of the first script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let b") - 1).toSource(), "({start:85, length:13, index:1})",
    "The left edge of the second script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let c") - 1).toSource(), "({start:151, length:13, index:2})",
    "The left edge of the third script was interpreted correctly.");

  is(parsed.getScriptInfo(source.indexOf("let a") - 2).toSource(), "({start:-1, length:-1, index:-1})",
    "The left outside of the first script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let b") - 2).toSource(), "({start:-1, length:-1, index:-1})",
    "The left outside of the second script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let c") - 2).toSource(), "({start:-1, length:-1, index:-1})",
    "The left outside of the third script was interpreted correctly.");

  is(parsed.getScriptInfo(source.indexOf("let a") + 12).toSource(), "({start:31, length:13, index:0})",
    "The right edge of the first script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let b") + 12).toSource(), "({start:85, length:13, index:1})",
    "The right edge of the second script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let c") + 12).toSource(), "({start:151, length:13, index:2})",
    "The right edge of the third script was interpreted correctly.");

  is(parsed.getScriptInfo(source.indexOf("let a") + 13).toSource(), "({start:-1, length:-1, index:-1})",
    "The right outside of the first script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let b") + 13).toSource(), "({start:-1, length:-1, index:-1})",
    "The right outside of the second script was interpreted correctly.");
  is(parsed.getScriptInfo(source.indexOf("let c") + 13).toSource(), "({start:-1, length:-1, index:-1})",
    "The right outside of the third script was interpreted correctly.");

  finish();
}
