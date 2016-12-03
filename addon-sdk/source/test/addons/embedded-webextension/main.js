const tabs = require("sdk/tabs");
const webExtension = require('sdk/webextension');

exports.testEmbeddedWebExtensionModuleInitializedException = function (assert) {
  let actualErr;

  assert.throws(
    () => webExtension.initFromBootstrapAddonParam({webExtension: null}),
    /'sdk\/webextension' module has been already initialized/,
    "Got the expected exception if the module is initialized twice"
  );
};

exports.testEmbeddedWebExtensionBackgroungPage = function* (assert) {
  try {
    const api = yield webExtension.startup();
    assert.ok(api, `webextension waitForStartup promise successfully resolved`);

    const apiSecondStartup = yield webExtension.startup();
    assert.equal(api, apiSecondStartup, "Got the same API object from the second startup call");

    const {browser} = api;

    let messageListener;
    let waitForBackgroundPageMessage = new Promise((resolve, reject) => {
      let numExpectedMessage = 2;
      messageListener = (msg, sender, sendReply) => {
        numExpectedMessage -= 1;
        if (numExpectedMessage == 1) {
          assert.equal(msg, "bg->sdk message",
                       "Got the expected message from the background page");
          sendReply("sdk reply");
        } else if (numExpectedMessage == 0) {
          assert.equal(msg, "sdk reply",
                       "The background page received the expected reply message");
          resolve();
        } else {
          console.error("Unexpected message received", {msg,sender, numExpectedMessage});
          assert.ok(false, `unexpected message received`);
          reject();
        }
      };
      browser.runtime.onMessage.addListener(messageListener);
    });

    let portListener;
    let waitForBackgroundPagePort = new Promise((resolve, reject) => {
      portListener = (port) => {
        let numExpectedMessages = 2;
        port.onMessage.addListener((msg) => {
          numExpectedMessages -= 1;

          if (numExpectedMessages == 1) {
            // Check that the legacy context has been able to receive the first port message
            // and reply with a port message to the background page.
            assert.equal(msg, "bg->sdk port message",
                         "Got the expected port message from the background page");
            port.postMessage("sdk->bg port message");
          } else if (numExpectedMessages == 0) {
            // Check that the background page has received the above port message.
            assert.equal(msg, "bg received sdk->bg port message",
                         "The background page received the expected port message");
          }
        });

        port.onDisconnect.addListener(() => {
          assert.equal(numExpectedMessages, 0, "Got the expected number of port messages");
          resolve();
        });
      };
      browser.runtime.onConnect.addListener(portListener);
    });

    yield Promise.all([
      waitForBackgroundPageMessage,
      waitForBackgroundPagePort,
    ]).then(() => {
      browser.runtime.onMessage.removeListener(messageListener);
      browser.runtime.onConnect.removeListener(portListener);
    });

  } catch (err) {
    assert.fail(`Unexpected webextension startup exception: ${err} - ${err.stack}`);
  }
};

exports.testEmbeddedWebExtensionContentScript = function* (assert, done) {
  try {
    const {browser} = yield webExtension.startup();
    assert.ok(browser, `webextension startup promise resolved successfully to the API object`);

    let messageListener;
    let waitForContentScriptMessage = new Promise((resolve, reject) => {
      let numExpectedMessage = 2;
      messageListener = (msg, sender, sendReply) => {
        numExpectedMessage -= 1;
        if (numExpectedMessage == 1) {
          assert.equal(msg, "content script->sdk message",
                       "Got the expected message from the content script");
          sendReply("sdk reply");
        } else if (numExpectedMessage == 0) {
          assert.equal(msg, "sdk reply",
                       "The content script received the expected reply message");
          resolve();
        } else {
          console.error("Unexpected message received", {msg,sender, numExpectedMessage});
          assert.ok(false, `unexpected message received`);
          reject();
        }
      };
      browser.runtime.onMessage.addListener(messageListener);
    });

    let portListener;
    let waitForContentScriptPort = new Promise((resolve, reject) => {
      portListener = (port) => {
        let numExpectedMessages = 2;
        port.onMessage.addListener((msg) => {
          numExpectedMessages -= 1;

          if (numExpectedMessages == 1) {
            assert.equal(msg, "content script->sdk port message",
                         "Got the expected message from the content script port");
            port.postMessage("sdk->content script port message");
          } else if (numExpectedMessages == 0) {
            assert.equal(msg, "content script received sdk->content script port message",
                         "The content script received the expected port message");
          }
        });
        port.onDisconnect.addListener(() => {
          assert.equal(numExpectedMessages, 0, "Got the epected number of port messages");
          resolve();
        });
      };
      browser.runtime.onConnect.addListener(portListener);
    });

    let url = "http://example.org/";

    var openedTab;
    tabs.once('open', function onOpen(tab) {
      openedTab = tab;
    });
    tabs.open(url);

    yield Promise.all([
      waitForContentScriptMessage,
      waitForContentScriptPort,
    ]).then(() => {
      browser.runtime.onMessage.removeListener(messageListener);
      browser.runtime.onConnect.removeListener(portListener);
      openedTab.close();
    });
  } catch (err) {
    assert.fail(`Unexpected webextension startup exception: ${err} - ${err.stack}`);
  }
};

require("sdk/test/runner").runTestsFromModule(module);
