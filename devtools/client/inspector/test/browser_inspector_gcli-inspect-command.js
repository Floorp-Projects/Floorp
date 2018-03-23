/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint key-spacing: 0 */
/* import-globals-from ../../commandline/test/helpers.js */

"use strict";

// Import the GCLI test helper
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/commandline/test/helpers.js",
  this);

// Testing that the gcli 'inspect' command works as it should.

const TEST_URI = URL_ROOT + "doc_inspector_gcli-inspect-command.html";

add_task(function* () {
  return helpers.addTabWithToolbar(TEST_URI, Task.async(function* (options) {
    let {inspector} = yield openInspector();

    let checkSelection = Task.async(function* (selector) {
      let node = yield getNodeFront(selector, inspector);
      is(inspector.selection.nodeFront, node, "the current selection is correct");
    });

    yield helpers.audit(options, [
      {
        setup: "inspect",
        check: {
          input:  "inspect",
          hints:         " <selector>",
          markup: "VVVVVVV",
          status: "ERROR",
          args: {
            selector: {
              message: "Value required for \u2018selector\u2019."
            },
          }
        },
      },
      {
        setup: "inspect div",
        check: {
          input:  "inspect div",
          hints:             "",
          markup: "VVVVVVVVVVV",
          status: "VALID",
          args: {
            selector: { message: "" },
          }
        },
        exec: {},
        post: () => checkSelection("div"),
      },
      {
        setup: "inspect .someclass",
        check: {
          input:  "inspect .someclass",
          hints:                    "",
          markup: "VVVVVVVVVVVVVVVVVV",
          status: "VALID",
          args: {
            selector: { message: "" },
          }
        },
        exec: {},
        post: () => checkSelection(".someclass"),
      },
      {
        setup: "inspect #someid",
        check: {
          input:  "inspect #someid",
          hints:                 "",
          markup: "VVVVVVVVVVVVVVV",
          status: "VALID",
          args: {
            selector: { message: "" },
          }
        },
        exec: {},
        post: () => checkSelection("#someid"),
      },
      {
        setup: "inspect button[disabled]",
        check: {
          input:  "inspect button[disabled]",
          hints:                          "",
          markup: "VVVVVVVVVVVVVVVVVVVVVVVV",
          status: "VALID",
          args: {
            selector: { message: "" },
          }
        },
        exec: {},
        post: () => checkSelection("button[disabled]"),
      },
      {
        setup: "inspect p>strong",
        check: {
          input:  "inspect p>strong",
          hints:                  "",
          markup: "VVVVVVVVVVVVVVVV",
          status: "VALID",
          args: {
            selector: { message: "" },
          }
        },
        exec: {},
        post: () => checkSelection("p>strong"),
      },
      {
        setup: "inspect :root",
        check: {
          input:  "inspect :root",
          hints:               "",
          markup: "VVVVVVVVVVVVV",
          status: "VALID"
        },
        exec: {},
        post: () => checkSelection(":root"),
      },
    ]);
  }));
});
