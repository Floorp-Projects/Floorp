/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// A helper frame-script for devtools/client/framework service worker tests.

/* eslint-env mozilla/frame-script */

"use strict";

function addMessageForwarder(name) {
  addMessageListener(name, function(_) {
    var channel = new MessageChannel();
    content.postMessage(name, "*", [channel.port2]);
    channel.port1.onmessage = function(msg) {
      sendAsyncMessage(name, msg.data);
      channel.port1.close();
    };
  });
}

const messages = [
  "devtools:sw-test:register",
  "devtools:sw-test:unregister",
  "devtools:sw-test:iframe:register-and-unregister",
];

for (var msg of messages) {
  addMessageForwarder(msg);
}
