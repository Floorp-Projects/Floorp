/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_policy_hardware_acceleration() {
  let winUtils = Services.wm.getMostRecentWindow("").windowUtils;
  let layerManager = winUtils.layerManagerType;
  ok(
    layerManager == "Basic" || layerManager == "WebRender (Software)",
    "Hardware acceleration disabled"
  );
});
