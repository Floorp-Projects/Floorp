/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../head.js */

add_task(async function capture() {
  let setsEnv = env.get("MOZSCREENSHOTS_SETS");
  if (!setsEnv) {
    ok(
      true,
      "MOZSCREENSHOTS_SETS wasn't specified so there's nothing to capture"
    );
    return;
  }

  let sets = TestRunner.splitEnv(setsEnv.trim());
  await TestRunner.start(sets);
});
