/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

add_task(async function test() {
  info("Test that about:profiling can modify the sampling interval.");

  await withAboutProfiling(async document => {
    const intervalInput = await getNearestInputFromText(
      document,
      "Sampling interval:"
    );
    is(
      getActiveConfiguration().interval,
      1,
      "The active configuration's interval is set to a specific number initially."
    );
    is(
      intervalInput.getAttribute("aria-valuemin"),
      "0.01",
      "aria-valuemin has the expected value"
    );
    is(
      intervalInput.getAttribute("aria-valuemax"),
      "100",
      "aria-valuemax has the expected value"
    );
    is(
      intervalInput.getAttribute("aria-valuenow"),
      "1",
      "aria-valuenow has the expected value"
    );

    info(
      "Increase the interval by an arbitrary amount. The input range will " +
        "scale that to the final value presented to the profiler."
    );
    setReactFriendlyInputValue(intervalInput, Number(intervalInput.value) + 1);

    is(
      getActiveConfiguration().interval,
      2,
      "The configuration's interval was able to be increased."
    );
    is(
      intervalInput.getAttribute("aria-valuenow"),
      "2",
      "aria-valuenow has the expected value"
    );

    intervalInput.focus();

    info("Increase the interval with the keyboard");
    EventUtils.synthesizeKey("VK_RIGHT");
    await waitUntil(() => getActiveConfiguration().interval === 3);
    is(
      intervalInput.getAttribute("aria-valuenow"),
      "3",
      "aria-valuenow has the expected value"
    );

    info("Decrease the interval with the keyboard");
    EventUtils.synthesizeKey("VK_LEFT");
    await waitUntil(() => getActiveConfiguration().interval === 2);
    is(
      intervalInput.getAttribute("aria-valuenow"),
      "2",
      "aria-valuenow has the expected value"
    );
  });
});
