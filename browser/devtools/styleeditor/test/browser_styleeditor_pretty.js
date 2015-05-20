/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that minified sheets are automatically prettified but other are left
// untouched.

const TESTCASE_URI = TEST_BASE_HTTP + "minified.html";

const PRETTIFIED_SOURCE = "" +
"body \{\r?\n" +                   // body{
  "\tbackground\:white;\r?\n" +   //   background:white;
"\}\r?\n" +                       // }
"div \{\r?\n" +                    // div{
  "\tfont\-size\:4em;\r?\n" +     //   font-size:4em;
  "\tcolor\:red\r?\n" +           //   color:red
"\}\r?\n" +                       // }
"span \{\r?\n" +                   // span{
  "\tcolor\:green;\r?\n"          //   color:green;
"\}\r?\n";                        // }

const ORIGINAL_SOURCE = "" +
"body \{ background\: red; \}\r?\n" + // body { background: red; }
"div \{\r?\n" +                       // div {
  "font\-size\: 5em;\r?\n" +          // font-size: 5em;
  "color\: red\r?\n" +                // color: red
"\}";                                 // }

add_task(function* () {
  let { ui } = yield openStyleEditorForURL(TESTCASE_URI);
  is(ui.editors.length, 2, "Two sheets present.");

  info("Testing minified style sheet.");
  let editor = yield ui.editors[0].getSourceEditor();

  let prettifiedSourceRE = new RegExp(PRETTIFIED_SOURCE);
  ok(prettifiedSourceRE.test(editor.sourceEditor.getText()),
     "minified source has been prettified automatically");

  info("Selecting second, non-minified style sheet.");
  yield ui.selectStyleSheet(ui.editors[1].styleSheet);

  editor = ui.editors[1];

  let originalSourceRE = new RegExp(ORIGINAL_SOURCE);
  ok(originalSourceRE.test(editor.sourceEditor.getText()),
     "non-minified source has been left untouched");
});
