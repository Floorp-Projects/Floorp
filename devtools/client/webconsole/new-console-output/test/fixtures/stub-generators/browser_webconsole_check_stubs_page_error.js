/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/osfile.jsm", {});

add_task(function* () {
  // On e10s, the exception is triggered in child process
  // and is ignored by test harness
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    expectUncaughtException();
  }

  let generatedStubs = yield generatePageErrorStubs();

  let repoStubFilePath = getTestFilePath("../stubs/pageError.js");
  let repoStubFileContent = yield OS.File.read(repoStubFilePath, { encoding: "utf-8" });

  if (generatedStubs != repoStubFileContent) {
    ok(false, "The pageError stubs file needs to be updated by running " +
      "`mach test devtools/client/webconsole/new-console-output/test/fixtures/" +
      "stub-generators/browser_webconsole_update_stubs_page_error.js`");
  } else {
    ok(true, "The pageError stubs file is up to date");
  }
});
