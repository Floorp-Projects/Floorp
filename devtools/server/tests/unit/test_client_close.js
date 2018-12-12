/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(threadClientTest(({ client }) => {
  return new Promise(resolve => {
    // Check that, if we fake a transport shutdown
    // (like if a device is unplugged)
    // the client is automatically closed,
    // and we can still call client.close.
    const onClosed = function() {
      client.removeListener("closed", onClosed);
      ok(true, "Client emitted 'closed' event");
      client.close().then(function() {
        ok(true, "client.close() successfully called its callback");
        resolve();
      });
    };
    client.addListener("closed", onClosed);
    client.transport.close();
  });
}));
