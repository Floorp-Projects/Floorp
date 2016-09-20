/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-console-api.html";

const { pageError: snippets} = require("devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/stub-snippets.js");

let stubs = {
  preparedMessages: [],
  packets: [],
};

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  ok(true, "make the test not fail");

  for (var [key,code] of snippets) {
    OS.File.writeAtomic(TEMP_FILE_PATH, `${code}`);
    let received = new Promise(resolve => {
      toolbox.target.client.addListener("pageError", function onPacket(e, packet) {
        toolbox.target.client.removeListener("pageError", onPacket);
        info("Received page error:" + e + " " + JSON.stringify(packet, null, "\t"));

        let message = prepareMessage(packet, {getNextId: () => 1});
        stubs.packets.push(formatPacket(message.messageText, packet));
        stubs.preparedMessages.push(formatStub(message.messageText, packet));
        resolve();
      });
    });

    yield ContentTask.spawn(gBrowser.selectedBrowser, code, function(code) {
      content.wrappedJSObject.location.reload();
    });

    yield received;
  }

  let filePath = OS.Path.join(`${BASE_PATH}/stubs`, "pageError.js");
  OS.File.writeAtomic(filePath, formatFile(stubs));
  OS.File.writeAtomic(TEMP_FILE_PATH, "");
});
