/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info("Test that about:profiling can modify the sampling interval.");

  await withAboutProfiling(async document => {
    is(
      getActiveConfiguration().interval,
      1,
      "The active configuration's interval is set to a specific number initially."
    );

    info(
      "Increase the interval by an arbitrary amount. The input range will " +
        "scale that to the final value presented to the profiler."
    );
    const intervalInput = await getNearestInputFromText(
      document,
      "Sampling interval:"
    );
    setReactFriendlyInputValue(intervalInput, Number(intervalInput.value) + 8);

    is(
      getActiveConfiguration().interval,
      2,
      "The configuration's interval was able to be increased."
    );
  });
});
