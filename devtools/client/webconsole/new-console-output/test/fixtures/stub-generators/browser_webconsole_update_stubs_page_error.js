/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
const TEST_URI = "data:text/html;charset=utf-8,stub generation";

const { pageError: snippets} = require("devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/stub-snippets.js");

let stubs = [];

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  ok(true, "make the test not fail");

  for (var [key,code] of snippets) {
    let received = new Promise(resolve => {
      toolbox.target.client.addListener("pageError", function onPacket(e, packet) {
        toolbox.target.client.removeListener("pageError", onPacket);
        info("Received page error:" + e + " " + JSON.stringify(packet, null, "\t"));

        let message = prepareMessage(packet, {getNextId: () => 1});
        stubs.push(formatStub(message.messageText, packet));
        resolve();
      });
    });

    info("Injecting script: " + code);

    yield ContentTask.spawn(gBrowser.selectedBrowser, code, function(code) {
      let container = content.document.createElement("script");
      content.document.body.appendChild(container);
      container.textContent = code;
      content.document.body.removeChild(container);
    });

    yield received;
  }

  let filePath = OS.Path.join(`${BASE_PATH}/stubs`, "pageError.js");
  OS.File.writeAtomic(filePath, formatFile(stubs));
});
