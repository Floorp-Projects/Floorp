add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["http://mochi.test/"]
    },

    background: function() {
      var ports_received = 0;
      var port_messages_received = 0;

      browser.runtime.onConnect.addListener((port) => {
        browser.test.assertTrue(!!port, "port1 received");

        ports_received++;
        browser.test.assertEq(1, ports_received, "1 port received");

        port.onMessage.addListener((msg, sender) => {
          browser.test.assertEq("port message", msg, "listener1 port message received");

          port_messages_received++
          browser.test.assertEq(1, port_messages_received, "1 port message received");
        })
      });
      browser.runtime.onConnect.addListener((port) => {
        browser.test.assertTrue(!!port, "port2 received");

        ports_received++;
        browser.test.assertEq(2, ports_received, "2 ports received");

        port.onMessage.addListener((msg, sender) => {
          browser.test.assertEq("port message", msg, "listener2 port message received");

          port_messages_received++
          browser.test.assertEq(2, port_messages_received, "2 port messages received");

          browser.test.notifyPass("contentscript_connect.pass");
        });
      });

      browser.tabs.executeScript({ file: "script.js" });
    },

    files: {
      "script.js": function() {
        var port = browser.runtime.connect();
        port.postMessage("port message");
      }
    }
  });

  yield extension.startup();
  yield extension.awaitFinish("contentscript_connect.pass");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
