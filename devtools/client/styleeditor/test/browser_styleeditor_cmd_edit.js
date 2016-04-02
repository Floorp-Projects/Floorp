/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the edit command works

// Import the GCLI test helper
/* import-globals-from ../../commandline/test/helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/commandline/test/helpers.js",
  this);

const TEST_URI = "http://example.com/browser/devtools/client/styleeditor/" +
                 "test/browser_styleeditor_cmd_edit.html";

add_task(function* () {
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: "edit",
      check: {
        input: "edit",
        hints: " <resource> [line]",
        markup: "VVVV",
        status: "ERROR",
        args: {
          resource: { status: "INCOMPLETE" },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit i",
      check: {
        input: "edit i",
        hints: "nline-css [line]",
        markup: "VVVVVI",
        status: "ERROR",
        args: {
          resource: { arg: " i", status: "INCOMPLETE" },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit c",
      check: {
        input: "edit c",
        hints: "ss#style2 [line]",
        markup: "VVVVVI",
        status: "ERROR",
        args: {
          resource: { arg: " c", status: "INCOMPLETE" },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit http",
      check: {
        input: "edit http",
        hints: "://example.com/browser/devtools/client/styleeditor/test/" +
               "resources_inpage1.css [line]",
        markup: "VVVVVIIII",
        status: "ERROR",
        args: {
          resource: {
            arg: " http",
            status: "INCOMPLETE",
            message: "Value required for \u2018resource\u2019."
          },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit page1",
      check: {
        input: "edit page1",
        hints: " [line] -> http://example.com/browser/devtools/client/" +
               "styleeditor/test/resources_inpage1.css",
        markup: "VVVVVIIIII",
        status: "ERROR",
        args: {
          resource: {
            arg: " page1",
            status: "INCOMPLETE",
            message: "Value required for \u2018resource\u2019."
          },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit page2",
      check: {
        input: "edit page2",
        hints: " [line] -> http://example.com/browser/devtools/client/" +
               "styleeditor/test/resources_inpage2.css",
        markup: "VVVVVIIIII",
        status: "ERROR",
        args: {
          resource: {
            arg: " page2",
            status: "INCOMPLETE",
            message: "Value required for \u2018resource\u2019."
          },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit stylez",
      check: {
        input: "edit stylez",
        hints: " [line]",
        markup: "VVVVVEEEEEE",
        status: "ERROR",
        args: {
          resource: {
            arg: " stylez",
            status: "ERROR", message: "Can\u2019t use \u2018stylez\u2019." },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit css#style2",
      check: {
        input: "edit css#style2",
        hints: " [line]",
        markup: "VVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          resource: { arg: " css#style2", status: "VALID", message: "" },
          line: { status: "VALID" },
        }
      },
    },
    {
      setup: "edit css#style2 5",
      check: {
        input: "edit css#style2 5",
        hints: "",
        markup: "VVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          resource: { arg: " css#style2", status: "VALID", message: "" },
          line: { value: 5, arg: " 5", status: "VALID" },
        }
      },
    },
    {
      setup: "edit css#style2 0",
      check: {
        input: "edit css#style2 0",
        hints: "",
        markup: "VVVVVVVVVVVVVVVVE",
        status: "ERROR",
        args: {
          resource: { arg: " css#style2", status: "VALID", message: "" },
          line: {
            arg: " 0",
            status: "ERROR",
            message: "0 is smaller than minimum allowed: 1."
          },
        }
      },
    },
    {
      setup: "edit css#style2 -1",
      check: {
        input: "edit css#style2 -1",
        hints: "",
        markup: "VVVVVVVVVVVVVVVVEE",
        status: "ERROR",
        args: {
          resource: { arg: " css#style2", status: "VALID", message: "" },
          line: {
            arg: " -1",
            status: "ERROR",
            message: "-1 is smaller than minimum allowed: 1."
          },
        }
      },
    }
  ]);

  let toolbox = gDevTools.getToolbox(options.target);
  ok(toolbox == null, "toolbox is closed");

  yield helpers.audit(options, [
    {
      setup: "edit css#style2",
      check: {
        input: "edit css#style2",
      },
      exec: { output: "" }
    },
  ]);

  toolbox = gDevTools.getToolbox(options.target);
  ok(toolbox != null, "toolbox is open");

  let styleEditor = toolbox.getCurrentPanel();
  ok(typeof styleEditor.selectStyleSheet === "function", "styleeditor is open");

  yield toolbox.destroy();

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
});
