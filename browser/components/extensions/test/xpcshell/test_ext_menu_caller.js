/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_create_menu_ext_error() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    async background() {
      let { fileName } = new Error();
      browser.menus.create({
        id: "muted-tab",
        title: "open link with Menu 1",
        contexts: ["link"],
      });
      await new Promise(resolve => {
        browser.menus.create(
          {
            id: "muted-tab",
            title: "open link with Menu 2",
            contexts: ["link"],
          },
          resolve
        );
      });
      browser.test.sendMessage("fileName", fileName);
    },
  });

  let fileName;
  const { messages } = await promiseConsoleOutput(async () => {
    await extension.startup();
    fileName = await extension.awaitMessage("fileName");
    await extension.unload();
  });
  let [msg] = messages
    .filter(m => m.message.includes("Unchecked lastError"))
    .map(m => m.QueryInterface(Ci.nsIScriptError));
  equal(msg.sourceName, fileName, "Message source");

  equal(
    msg.errorMessage,
    "Unchecked lastError value: Error: ID already exists: muted-tab",
    "Message content"
  );
  equal(msg.lineNumber, 9, "Message line");

  let frame = msg.stack;
  equal(frame.source, fileName, "Frame source");
  equal(frame.line, 9, "Frame line");
  equal(frame.column, 23, "Frame column");
});
