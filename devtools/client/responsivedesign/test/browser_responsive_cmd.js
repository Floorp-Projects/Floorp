/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// /////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("destroy");

function test() {
  let manager = ResponsiveUIManager;
  let done;

  function isOpen() {
    return gBrowser.getBrowserContainer(gBrowser.selectedBrowser)
                   .hasAttribute("responsivemode");
  }

  helpers.addTabWithToolbar("data:text/html;charset=utf-8,hi", (options) => {
    return helpers.audit(options, [
      {
        setup() {
          done = once(manager, "on");
          return helpers.setInput(options, "resize toggle");
        },
        check: {
          input:  "resize toggle",
          hints:               "",
          markup: "VVVVVVVVVVVVV",
          status: "VALID"
        },
        exec: {
          output: ""
        },
        post: Task.async(function* () {
          yield done;
          ok(isOpen(), "responsive mode is open");
        }),
      },
      {
        setup() {
          done = once(manager, "off");
          return helpers.setInput(options, "resize toggle");
        },
        check: {
          input:  "resize toggle",
          hints:               "",
          markup: "VVVVVVVVVVVVV",
          status: "VALID"
        },
        exec: {
          output: ""
        },
        post: Task.async(function* () {
          yield done;
          ok(!isOpen(), "responsive mode is closed");
        }),
      },
      {
        setup() {
          done = once(manager, "on");
          return helpers.setInput(options, "resize on");
        },
        check: {
          input:  "resize on",
          hints:           "",
          markup: "VVVVVVVVV",
          status: "VALID"
        },
        exec: {
          output: ""
        },
        post: Task.async(function* () {
          yield done;
          ok(isOpen(), "responsive mode is open");
        }),
      },
      {
        setup() {
          done = once(manager, "off");
          return helpers.setInput(options, "resize off");
        },
        check: {
          input:  "resize off",
          hints:            "",
          markup: "VVVVVVVVVV",
          status: "VALID"
        },
        exec: {
          output: ""
        },
        post: Task.async(function* () {
          yield done;
          ok(!isOpen(), "responsive mode is closed");
        }),
      },
      {
        setup() {
          done = once(manager, "on");
          return helpers.setInput(options, "resize to 400 400");
        },
        check: {
          input:  "resize to 400 400",
          hints:                   "",
          markup: "VVVVVVVVVVVVVVVVV",
          status: "VALID",
          args: {
            width: { value: 400 },
            height: { value: 400 },
          }
        },
        exec: {
          output: ""
        },
        post: Task.async(function* () {
          yield done;
          ok(isOpen(), "responsive mode is open");
        }),
      },
      {
        setup() {
          done = once(manager, "off");
          return helpers.setInput(options, "resize off");
        },
        check: {
          input:  "resize off",
          hints:            "",
          markup: "VVVVVVVVVV",
          status: "VALID"
        },
        exec: {
          output: ""
        },
        post: Task.async(function* () {
          yield done;
          ok(!isOpen(), "responsive mode is closed");
        }),
      },
    ]);
  }).then(finish);
}
