/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// A helper frame-script for devtools/client/framework service worker tests.

"use strict";

addMessageListener("devtools:sw-test:register", function (msg) {
  content.navigator.serviceWorker.register("serviceworker.js")
    .then(swr => {
      sendAsyncMessage("devtools:sw-test:register", {success: true});
    }, error => {
      sendAsyncMessage("devtools:sw-test:register", {success: false});
    });
});

addMessageListener("devtools:sw-test:unregister", function (msg) {
  content.navigator.serviceWorker.getRegistration().then(swr => {
    swr.unregister().then(result => {
      sendAsyncMessage("devtools:sw-test:unregister",
                       {success: result ? true : false});
    });
  });
});

addMessageListener("devtools:sw-test:iframe:register-and-unregister", function (msg) {
  var frame = content.document.createElement("iframe");
  frame.addEventListener("load", function onLoad() {
    frame.removeEventListener("load", onLoad);
    frame.contentWindow.navigator.serviceWorker.register("serviceworker.js")
      .then(swr => {
        return swr.unregister();
      }).then(_ => {
        frame.remove();
        sendAsyncMessage("devtools:sw-test:iframe:register-and-unregister",
                         {success: true});
      }).catch(error => {
        sendAsyncMessage("devtools:sw-test:iframe:register-and-unregister",
                         {success: false});
      });
  });
  frame.src = "browser_toolbox_options_enabled_serviceworkers_testing.html";
  content.document.body.appendChild(frame);
});
