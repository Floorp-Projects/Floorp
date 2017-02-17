/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/osfile.jsm", {});

add_task(function* () {
  let fileContent = yield generateCssMessageStubs();
  let filePath = OS.Path.join(`${BASE_PATH}/stubs`, "cssMessage.js");
  yield OS.File.writeAtomic(filePath, fileContent);
  ok(true, "Make the test not fail");
});
