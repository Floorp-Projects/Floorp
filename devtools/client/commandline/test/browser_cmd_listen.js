/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the listen/unlisten commands work as they should.

const TEST_URI = "http://example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_cookie.html";

function test() {
  return Task.spawn(testTask).then(finish, helpers.handleError);
}

var tests = {
  testInput: function (options) {
    return helpers.audit(options, [
      {
        setup:    "listen",
        check: {
          input:  "listen",
          markup: "VVVVVV",
          status: "VALID"
        },
      },
      {
        setup:    "unlisten",
        check: {
          input:  "unlisten",
          markup: "VVVVVVVV",
          status: "VALID"
        },
        exec: {
          output: "All TCP ports closed"
        }
      },
      {
        setup: function () {
          return helpers.setInput(options, "listen");
        },
        check: {
          input:  "listen",
          hints:        " [port]",
          markup: "VVVVVV",
          status: "VALID"
        },
        exec: {
          output: "Listening on port 6080"
        }
      },
      {
        setup: function () {
          return helpers.setInput(options, "listen 8000");
        },
        exec: {
          output: "Listening on port 8000"
        }
      },
      {
        setup: function () {
          return helpers.setInput(options, "unlisten");
        },
        exec: {
          output: "All TCP ports closed"
        }
      }
    ]);
  },
};

function* testTask() {
  Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  yield helpers.runTests(options, tests);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
  Services.prefs.clearUserPref("devtools.debugger.remote-enabled");
}
