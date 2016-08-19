/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
const TEST_URI = "data:text/html;charset=utf-8,stub generation";

const { evaluationResult: snippets} = require("devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/stub-snippets.js");

let stubs = [];

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  ok(true, "make the test not fail");

  for (var [code,key] of snippets) {
    const packet = yield new Promise(resolve => {
      toolbox.target.activeConsole.evaluateJS(code, resolve);
    });
    stubs.push(formatStub(key, packet));
    if (stubs.length == snippets.size) {
      let filePath = OS.Path.join(`${BASE_PATH}/stubs`, "evaluationResult.js");
      OS.File.writeAtomic(filePath, formatFile(stubs));
    }
  }
});
