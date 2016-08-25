/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
const { consoleApi: snippets } = require("devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/stub-snippets.js");

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-console-api.html";

let stubs = {
  preparedMessages: [],
  packets: [],
};

add_task(function* () {
  let tempFilePath = OS.Path.join(`${BASE_PATH}/stub-generators`, "test-tempfile.js");
  for (var [key, {keys, code}] of snippets) {
    OS.File.writeAtomic(tempFilePath, `function triggerPacket() {${code}}`);
    let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
    let hud = toolbox.getCurrentPanel().hud;
    let {ui} = hud;

    ok(ui.jsterm, "jsterm exists");
    ok(ui.newConsoleOutput, "newConsoleOutput exists");

    let received = new Promise(resolve => {
      let i = 0;
      toolbox.target.client.addListener("consoleAPICall", (type, res) => {
        stubs.packets.push(formatPacket(keys[i], res));
        stubs.preparedMessages.push(formatStub(keys[i], res));
        if(++i === keys.length ){
          resolve();
        }
      });
    });

    yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function() {
      content.wrappedJSObject.triggerPacket();
    });

    yield received;
  }
  let filePath = OS.Path.join(`${BASE_PATH}/stubs`, "consoleApi.js");
  OS.File.writeAtomic(filePath, formatFile(stubs));
  OS.File.writeAtomic(tempFilePath, "");
});
