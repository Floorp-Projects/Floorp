/*
 * Test to ensure that load/decode notifications are delivered completely and
 * asynchronously when dealing with a file that's a 404.
 */
/* import-globals-from async_load_tests.js */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

var ioService = Services.io;

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return ioService.newURI(
    "http://localhost:" +
      server.identity.primaryPort +
      "/async-notification-never-here.jpg"
  );
});

load("async_load_tests.js");
