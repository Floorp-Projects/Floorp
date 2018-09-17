/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "Feeds" ];

var Feeds = {
  // Listeners are added in nsBrowserGlue.js
  receiveMessage(aMessage) {
    let data = aMessage.data;
    switch (aMessage.name) {
      case "WCCR:registerProtocolHandler": {
        let registrar = Cc["@mozilla.org/embeddor.implemented/web-content-handler-registrar;1"].
                          getService(Ci.nsIWebContentHandlerRegistrar);
        registrar.registerProtocolHandler(data.protocol, data.uri, data.title,
                                          aMessage.target);
        break;
      }
    }
  },
};
