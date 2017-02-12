/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint key-spacing: 0 */

// Tests the various highlight command parameters and options

requestLongerTimeout(3);

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
      setup: "highlight body --showall",
      check: {
        input:  "highlight body --showall",
        hints:                          " [options]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "1 node highlighted"
      }
    },
    {
      setup: "highlight body --keep",
      check: {
        input:  "highlight body --keep",
        hints:                       " [options]",
        markup: "VVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "1 node highlighted"
      }
    },
    {
      setup: "highlight body --hideguides --showinfobar --showall --region " +
        "content --fill red --keep",
      check: {
        input:  "highlight body --hideguides --showinfobar --showall --region " +
          "content --fill red --keep",
        hints: "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV" +
          "VVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "1 node highlighted"
      }
    },
    {
      setup: "highlight .item",
      check: {
        input:  "highlight .item",
        hints:                 " [options]",
        markup: "VVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "101 nodes matched, but only 100 nodes highlighted. Use " +
          "\u2018--showall\u2019 to show all"
      }
    },
    {
      setup: "highlight .item --showall",
      check: {
        input:  "highlight .item --showall",
        hints:                           " [options]",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "101 nodes highlighted"
      }
    }
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
