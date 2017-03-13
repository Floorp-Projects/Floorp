/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint key-spacing: 0 */

// Tests the various highlight command parameters and options

// Creating a test page with many elements to test the --showall option
var TEST_PAGE = "data:text/html;charset=utf-8,<body><ul>";
for (let i = 0; i < 101; i++) {
  TEST_PAGE += "<li class='item'>" + i + "</li>";
}
TEST_PAGE += "</ul></body>";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_PAGE);
  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: "highlight",
      check: {
        input:  "highlight",
        hints:           " [selector] [options]",
        markup: "VVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "0 nodes highlighted"
      }
    },
    {
      setup: "highlight bo",
      check: {
        input:  "highlight bo",
        hints:              " [options]",
        markup: "VVVVVVVVVVII",
        status: "ERROR"
      },
      exec: {
        output: "Error: No matches"
      }
    },
    {
      setup: "highlight body",
      check: {
        input:  "highlight body",
        hints:                " [options]",
        markup: "VVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "1 node highlighted"
      }
    },
    {
      setup: "highlight body --hideguides",
      check: {
        input:  "highlight body --hideguides",
        hints:                             " [options]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "1 node highlighted"
      }
    },
    {
      setup: "highlight body --showinfobar",
      check: {
        input:  "highlight body --showinfobar",
        hints:                              " [options]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "1 node highlighted"
      }
    }
  ]);

  // Hide all highlighters before finishing the test.
  yield helpers.audit(options, [{
    setup: "unhighlight",
    exec: {}
  }]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
