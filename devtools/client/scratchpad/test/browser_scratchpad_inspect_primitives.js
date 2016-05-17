/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that inspecting primitive values uses the object inspector, not an
// inline comment.

var {Task} = require("devtools/shared/task");

function test() {
  const options = {
    tabContent: "test inspecting primitive values"
  };
  openTabAndScratchpad(options)
    .then(Task.async(runTests))
    .then(finish, console.error);
}

function* runTests([win, sp]) {
  // Inspect a number.
  yield checkResults(sp, 7);

  // Inspect a string.
  yield checkResults(sp, "foobar", true);

  // Inspect a boolean.
  yield checkResults(sp, true);
}

// Helper function that does the actual testing.
var checkResults = Task.async(function* (sp, value, isString = false) {
  let sourceValue = value;
  if (isString) {
    sourceValue = '"' + value + '"';
  }
  let source = "var foobar = " + sourceValue + "; foobar";
  sp.setText(source);
  yield sp.inspect();

  let sidebar = sp.sidebar;
  ok(sidebar.visible, "sidebar is open");

  let found = false;

  outer: for (let scope of sidebar.variablesView) {
    for (let [, obj] of scope) {
      for (let [, prop] of obj) {
        if (prop.name == "value" && prop.value == value) {
          found = true;
          break outer;
        }
      }
    }
  }

  ok(found, "found the value of " + value);

  let tabbox = sidebar._sidebar._tabbox;
  ok(!tabbox.hasAttribute("hidden"), "Scratchpad sidebar visible");
  sidebar.hide();
  ok(tabbox.hasAttribute("hidden"), "Scratchpad sidebar hidden");
});
