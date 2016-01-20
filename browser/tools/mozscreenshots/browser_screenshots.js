/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);

add_task(function* test() {
  let { TestRunner } = Cu.import("chrome://mozscreenshots/content/TestRunner.jsm", {});
  let sets = ["TabsInTitlebar", "Tabs", "WindowSize", "Toolbars", "LightweightThemes"];
  let setsEnv = env.get("MOZSCREENSHOTS_SETS");
  if (setsEnv) {
    sets = setsEnv.trim().split(",");
  }

  yield TestRunner.start(sets);
});
