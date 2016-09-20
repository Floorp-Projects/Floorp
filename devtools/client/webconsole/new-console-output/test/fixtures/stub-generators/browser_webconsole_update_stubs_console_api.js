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
  for (var [key, {keys, code}] of snippets) {
    yield OS.File.writeAtomic(TEMP_FILE_PATH, `function triggerPacket() {${code}}`);

    let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
    let {ui} = toolbox.getCurrentPanel().hud;

    ok(ui.jsterm, "jsterm exists");
    ok(ui.newConsoleOutput, "newConsoleOutput exists");

    let received = new Promise(resolve => {
      let i = 0;
      let listener = (type, res) => {
        stubs.packets.push(formatPacket(keys[i], res));
        stubs.preparedMessages.push(formatStub(keys[i], res));
        if(++i === keys.length ){
          toolbox.target.client.removeListener("consoleAPICall", listener);
          resolve();
        }
      };
      toolbox.target.client.addListener("consoleAPICall", listener);
    });

    yield ContentTask.spawn(gBrowser.selectedBrowser, key, function(key) {
      var script = content.document.createElement("script");
      script.src = "test-tempfile.js?key=" + encodeURIComponent(key);
      script.onload = function() { content.wrappedJSObject.triggerPacket(); }
      content.document.body.appendChild(script);
    });

    yield received;

    yield closeTabAndToolbox();
  }
  let filePath = OS.Path.join(`${BASE_PATH}/stubs`, "consoleApi.js");
  OS.File.writeAtomic(filePath, formatFile(stubs));
  OS.File.writeAtomic(TEMP_FILE_PATH, "");
});
