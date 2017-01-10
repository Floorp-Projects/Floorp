/*
 * Test to ensure that load/decode notifications are delivered completely and
 * asynchronously when dealing with a file that's a 404.
 */
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                           .getService(Components.interfaces.nsIIOService);

XPCOMUtils.defineLazyGetter(this, "uri", function() {
    return ioService.newURI("http://localhost:" +
                            server.identity.primaryPort +
                            "/async-notification-never-here.jpg");
});

load('async_load_tests.js');
