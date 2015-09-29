/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the highlight command, ensure no invalid arguments are given

const TEST_PAGE = "data:text/html;charset=utf-8,foo";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_PAGE);
  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: "measure",
      check: {
        input: "measure",
        markup: "VVVVVVV",
        status: "VALID"
      }
    },
    {
      setup: "measure on",
      check: {
        input: "measure on",
        markup: "VVVVVVVVEE",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    },
    {
      setup: "measure --visible",
      check: {
        input: "measure --visible",
        markup: "VVVVVVVVEEEEEEEEE",
        status: "ERROR"
      },
      exec: {
        output: "Error: Too many arguments"
      }
    }
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
