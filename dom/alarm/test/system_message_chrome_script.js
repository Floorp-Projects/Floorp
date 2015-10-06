/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
'use strict';

var { classes: Cc, interfaces: Ci } = Components;

const systemMessenger = Cc["@mozilla.org/system-message-internal;1"]
                          .getService(Ci.nsISystemMessagesInternal);
const ioService = Cc["@mozilla.org/network/io-service;1"]
                    .getService(Ci.nsIIOService);

addMessageListener("trigger-register-page", function(aData) {
  systemMessenger.registerPage(aData.type,
                               ioService.newURI(aData.pageURL, null, null),
                               ioService.newURI(aData.manifestURL, null, null));
  sendAsyncMessage("page-registered");
});
