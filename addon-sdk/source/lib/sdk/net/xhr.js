/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { deprecateFunction } = require("../util/deprecate");
const { Cc, Ci } = require("chrome");
const XMLHttpRequest = require("../addon/window").window.XMLHttpRequest;

Object.defineProperties(XMLHttpRequest.prototype, {
  mozBackgroundRequest: {
    value: true,
  },
  forceAllowThirdPartyCookie: {
    configurable: true,
    value: deprecateFunction(function() {
      forceAllowThirdPartyCookie(this);

    }, "`xhr.forceAllowThirdPartyCookie()` is deprecated, please use" +
       "`require('sdk/net/xhr').forceAllowThirdPartyCookie(request)` instead")
  }
});
exports.XMLHttpRequest = XMLHttpRequest;

function forceAllowThirdPartyCookie(xhr) {
  if (xhr.channel instanceof Ci.nsIHttpChannelInternal)
    xhr.channel.forceAllowThirdPartyCookie = true;
}
exports.forceAllowThirdPartyCookie = forceAllowThirdPartyCookie;

// No need to handle add-on unloads as addon/window is closed at unload
// and it will take down all the associated requests.