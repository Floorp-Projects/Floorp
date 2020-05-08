/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test that about:profiling can modify the sampling interval.");

  await withAboutProfiling(async document => {
    is(
      getActiveConfiguration().capacity,
      128 * 1024 * 1024,
      "The active configuration is set to a specific number initially. If this" +
        " test fails here, then the magic numbers here may need to be adjusted."
    );

    info("Change the buffer input to an arbitrarily smaller value.");
    const bufferInput = await getNearestInputFromText(document, "Buffer size:");
    setReactFriendlyInputValue(bufferInput, Number(bufferInput.value) * 0.1);

    is(
      getActiveConfiguration().capacity,
      256 * 1024,
      "The capacity changed to a smaller value."
    );
  });
});
