/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the various highlight command parameters and options that doesn't
// involve nodes at all.

var TEST_PAGE = "data:text/html;charset=utf-8,";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_PAGE);
  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: "highlight body --hide",
      check: {
        input:  "highlight body --hide",
        hints:                       "guides [options]",
        markup: "VVVVVVVVVVVVVVVIIIIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    },
    {
      setup: "highlight body --show",
      check: {
        input:  "highlight body --show",
        hints:                       "infobar [options]",
        markup: "VVVVVVVVVVVVVVVIIIIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    },
    {
      setup: "highlight body --showa",
      check: {
        input:  "highlight body --showa",
        hints:                        "ll [options]",
        markup: "VVVVVVVVVVVVVVVIIIIIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    },
    {
      setup: "highlight body --r",
      check: {
        input:  "highlight body --r",
        hints:                    "egion [options]",
        markup: "VVVVVVVVVVVVVVVIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    },
    {
      setup: "highlight body --region",
      check: {
        input:  "highlight body --region",
        hints:                         " <selection> [options]",
        markup: "VVVVVVVVVVVVVVVIIIIIIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Value required for \u2018region\u2019."
      }
    },
    {
      setup: "highlight body --fi",
      check: {
        input:  "highlight body --fi",
        hints:                     "ll [options]",
        markup: "VVVVVVVVVVVVVVVIIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    },
    {
      setup: "highlight body --fill",
      check: {
        input:  "highlight body --fill",
        hints:                       " <string> [options]",
        markup: "VVVVVVVVVVVVVVVIIIIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Value required for \u2018fill\u2019."
      }
    },
    {
      setup: "highlight body --ke",
      check: {
        input:  "highlight body --ke",
        hints:                     "ep [options]",
        markup: "VVVVVVVVVVVVVVVIIII",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    },
    {
      setup: "unhighlight",
      check: {
        input:  "unhighlight",
        hints:  "",
        markup: "VVVVVVVVVVV",
        status: "VALID"
      }
    }
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
