/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Record allocations while reloading the page with the webconsole opened

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/test/allocations/reload-test.js",
  this
);

add_task(createPanelReloadTest("reload-webconsole", "webconsole"));
