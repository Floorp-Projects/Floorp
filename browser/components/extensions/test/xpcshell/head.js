"use strict";

/* exported createHttpServer, promiseConsoleOutput  */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  HttpServer: "resource://testing-common/httpd.js",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
});

ExtensionTestUtils.init(this);

/**
 * Creates a new HttpServer for testing, and begins listening on the
 * specified port. Automatically shuts down the server when the test
 * unit ends.
 *
 * @param {integer} [port]
 *        The port to listen on. If omitted, listen on a random
 *        port. The latter is the preferred behavior.
 *
 * @returns {HttpServer}
 */
function createHttpServer(port = -1) {
  let server = new HttpServer();
  server.start(port);

  registerCleanupFunction(() => {
    return new Promise(resolve => {
      server.stop(resolve);
    });
  });

  return server;
}

var promiseConsoleOutput = async function(task) {
  const DONE = `=== console listener ${Math.random()} done ===`;

  let listener;
  let messages = [];
  let awaitListener = new Promise(resolve => {
    listener = msg => {
      if (msg == DONE) {
        resolve();
      } else {
        void (msg instanceof Ci.nsIConsoleMessage);
        messages.push(msg);
      }
    };
  });

  Services.console.registerListener(listener);
  try {
    let result = await task();

    Services.console.logStringMessage(DONE);
    await awaitListener;

    return { messages, result };
  } finally {
    Services.console.unregisterListener(listener);
  }
};
