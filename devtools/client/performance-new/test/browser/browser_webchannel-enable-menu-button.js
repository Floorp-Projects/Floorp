/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info("Test the WebChannel mechanism works for turning on the menu button.");
  await makeSureProfilerPopupIsDisabled();

  await withWebChannelTestDocument(async () => {
    await waitForTabTitle("WebChannel Page Ready");
    await waitForProfilerMenuButton();
    ok(true, "The profiler menu button was enabled by the WebChannel.");
  });
});
