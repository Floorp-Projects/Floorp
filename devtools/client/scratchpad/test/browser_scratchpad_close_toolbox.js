/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that closing the toolbox after having opened a scratchpad leaves the
// latter in a functioning state.

var {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
var {TargetFactory} = require("devtools/client/framework/target");

function test() {
  const options = {
    tabContent: "test closing toolbox and then reusing scratchpad"
  };
  openTabAndScratchpad(options)
    .then(Task.async(runTests))
    .then(finish, console.error);
}

function* runTests([win, sp]) {
  // Use the scratchpad before opening the toolbox.
  const source = "window.foobar = 7;";
  sp.setText(source);
  let [,, result] = yield sp.display();
  is(result, 7, "Display produced the expected output.");

  // Now open the toolbox and close it again.
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = yield gDevTools.showToolbox(target, "webconsole");
  ok(toolbox, "Toolbox was opened.");
  let closed = yield gDevTools.closeToolbox(target);
  is(closed, true, "Toolbox was closed.");

  // Now see if using the scratcphad works as expected.
  sp.setText(source);
  let [,, result2] = yield sp.display();
  is(result2, 7,
     "Display produced the expected output after the toolbox was gone.");
}
