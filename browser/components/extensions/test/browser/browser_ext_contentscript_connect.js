/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://mochi.test/"],
    },

    background: function() {
      let ports_received = 0;
      let port_messages_received = 0;

      browser.runtime.onConnect.addListener(port => {
        browser.test.assertTrue(!!port, "port1 received");

        ports_received++;
        browser.test.assertEq(1, ports_received, "1 port received");

        port.onMessage.addListener((msg, msgPort) => {
          browser.test.assertEq(
            "port message",
            msg,
            "listener1 port message received"
          );
          browser.test.assertEq(
            port,
            msgPort,
            "onMessage should receive port as second argument"
          );

          port_messages_received++;
          browser.test.assertEq(
            1,
            port_messages_received,
            "1 port message received"
          );
        });
      });
      browser.runtime.onConnect.addListener(port => {
        browser.test.assertTrue(!!port, "port2 received");

        ports_received++;
        browser.test.assertEq(2, ports_received, "2 ports received");

        port.onMessage.addListener((msg, msgPort) => {
          browser.test.assertEq(
            "port message",
            msg,
            "listener2 port message received"
          );
          browser.test.assertEq(
            port,
            msgPort,
            "onMessage should receive port as second argument"
          );

          port_messages_received++;
          browser.test.assertEq(
            2,
            port_messages_received,
            "2 port messages received"
          );

          browser.test.notifyPass("contentscript_connect.pass");
        });
      });

      browser.tabs.executeScript({ file: "script.js" }).catch(e => {
        browser.test.fail(`Error: ${e} :: ${e.stack}`);
        browser.test.notifyFail("contentscript_connect.pass");
      });
    },

    files: {
      "script.js": function() {
        let port = browser.runtime.connect();
        port.postMessage("port message");
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("contentscript_connect.pass");
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
