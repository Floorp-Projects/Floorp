/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/osfile.jsm");
const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/test-css-message.html";

const { cssMessage: snippets} = require("devtools/client/webconsole/new-console-output/test/fixtures/stub-generators/stub-snippets.js");

let stubs = {
  preparedMessages: [],
  packets: [],
};

add_task(function* () {
  let toolbox = yield openNewTabAndToolbox(TEST_URI, "webconsole");
  ok(true, "make the test not fail");

  for (let [key, code] of snippets) {
    OS.File.writeAtomic(TEMP_CSS_FILE_PATH, code);
    let received = new Promise(resolve => {
      /* CSS errors are considered as pageError on the server */
      toolbox.target.client.addListener("pageError", function onPacket(e, packet) {
        toolbox.target.client.removeListener("pageError", onPacket);
        info("Received css message:" + e + " " + JSON.stringify(packet, null, "\t"));

        let message = prepareMessage(packet, {getNextId: () => 1});
        stubs.packets.push(formatPacket(message.messageText, packet));
        stubs.preparedMessages.push(formatStub(message.messageText, packet));
        resolve();
      });
    });

    yield ContentTask.spawn(gBrowser.selectedBrowser, key, function (snippetKey) {
      let stylesheet = content.document.createElement("link");
      stylesheet.rel = "stylesheet";
      stylesheet.href = "test-tempfile.css?key=" + encodeURIComponent(snippetKey);
      content.document.body.appendChild(stylesheet);
    });

    yield received;
  }

  let filePath = OS.Path.join(`${BASE_PATH}/stubs`, "cssMessage.js");
  OS.File.writeAtomic(filePath, formatFile(stubs));
  OS.File.writeAtomic(TEMP_CSS_FILE_PATH, "");
});
