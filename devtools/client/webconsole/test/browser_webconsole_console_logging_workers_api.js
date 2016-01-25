/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the basic console.log()-style APIs and filtering work for
// sharedWorkers

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console-workers.html";

add_task(function*() {
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
