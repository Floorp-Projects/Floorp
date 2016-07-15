/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the qsa commands work as they should.

const TEST_URI = "data:text/html;charset=utf-8,<body></body>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function (options) {
    return helpers.audit(options, [
      {
        setup: "qsa",
        check: {
          input:  "qsa",
          hints:  " [query]",
          markup: "VVV",
          status: "VALID"
        }
      },
      {
        setup: "qsa body",
        check: {
          input:  "qsa body",
          hints:  "",
          markup: "VVVVVVVV",
          status: "VALID"
        }
      }
    ]);
  }).then(finish, helpers.handleError);
}
