/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// A helper frame-script for browser/devtools/framework service worker tests.

"use strict";

addMessageListener("devtools:sw-test:register", function(msg) {
  content.navigator.serviceWorker.register("serviceworker.js")
    .then(swr => {
      sendAsyncMessage("devtools:sw-test:register", {success: true});
    }, error => {
      sendAsyncMessage("devtools:sw-test:register", {success: false});
    });
});

addMessageListener("devtools:sw-test:unregister", function(msg) {
  content.navigator.serviceWorker.getRegistration().then(swr => {
    swr.unregister().then(result => {
      sendAsyncMessage("devtools:sw-test:unregister",
                       {success: result ? true : false});
    });
  });
});
