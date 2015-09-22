/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the paintflashing command correctly sets its state.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_cookie.html";

function test() {
  return Task.spawn(testTask).then(finish, helpers.handleError);
}

var tests = {
  testInput: function(options) {
    let toggleCommand = options.requisition.system.commands.get("paintflashing toggle");

    let _tab = options.tab;

    let actions = [
      {
        command: "paintflashing on",
        isChecked: true,
        label: "checked after on"
      },
      {
        command: "paintflashing off",
        isChecked: false,
        label: "unchecked after off"
      },
      {
        command: "paintflashing toggle",
        isChecked: true,
        label: "checked after toggle"
      },
      {
        command: "paintflashing toggle",
        isChecked: false,
        label: "unchecked after toggle"
      }
    ];

    return helpers.audit(options, actions.map(spec => ({
      setup: spec.command,
      exec: {},
      post: () => is(toggleCommand.state.isChecked({_tab}), spec.isChecked, spec.label)
    })));
  },
};

function* testTask() {
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  yield helpers.runTests(options, tests);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
