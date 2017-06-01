/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global addMessageListener, removeMessageListener */

"use strict";

const { utils: Cu } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

function onInit(message) {
  // Only reply if we are in a real content process
  if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
    let {init} = Cu.import("resource://devtools/server/content-server.jsm", {});
    init(message);
  }
}

function onClose() {
  removeMessageListener("debug:init-content-server", onInit);
  removeMessageListener("debug:close-content-server", onClose);
}

addMessageListener("debug:init-content-server", onInit);
addMessageListener("debug:close-content-server", onClose);
