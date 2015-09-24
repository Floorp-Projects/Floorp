/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the basic console.log()-style APIs and filtering work for
// sharedWorkers

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-workers.html";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  yield waitForMessages({
    webconsole: hud,
    messages: [{
      text: "foo-bar-shared-worker"
    }],
  });

  hud.setFilterState("sharedworkers", false);

  is(hud.outputNode.querySelectorAll(".filtered-by-type").length, 1,
     "1 message hidden for sharedworkers (logging turned off)");

  hud.setFilterState("sharedworkers", true);

  is(hud.outputNode.querySelectorAll(".filtered-by-type").length, 0,
     "1 message shown for sharedworkers (logging turned on)");

  hud.setFilterState("sharedworkers", false);

  hud.jsterm.clearOutput(true);
});
