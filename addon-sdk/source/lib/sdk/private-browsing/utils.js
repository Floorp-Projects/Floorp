/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci, Cu } = require('chrome');
const { defer } = require('../lang/functional');
const { emit, on, once, off } = require('../event/core');
const { when: unload } = require('../system/unload');
const events = require('../system/events');
const { deprecateFunction } = require('../util/deprecate');
const { isOneOf, is, satisfiesVersion, version } = require('../system/xul-app');
const { isWindowPrivate } = require('../window/utils');
const { isPrivateBrowsingSupported } = require('../self');

let deferredEmit = defer(emit);
let pbService;
let PrivateBrowsingUtils;

// Private browsing is only supported in Fx
if (isOneOf(['Firefox', 'Fennec'])) {
  // get the nsIPrivateBrowsingService if it exists
  try {
    pbService = Cc["@mozilla.org/privatebrowsing;1"].
                getService(Ci.nsIPrivateBrowsingService);

    // a dummy service exists for the moment (Fx20 atleast), but will be removed eventually
    // ie: the service will exist, but it won't do anything and the global private browing
    //     feature is not really active.  See Bug 818800 and Bug 826037
    if (!('privateBrowsingEnabled' in pbService))
      pbService = undefined;
  }
  catch(e) { /* Private Browsing Service has been removed (Bug 818800) */ }

  try {
    PrivateBrowsingUtils = Cu.import('resource://gre/modules/PrivateBrowsingUtils.jsm', {}).PrivateBrowsingUtils;
  }
  catch(e) { /* if this file DNE then an error will be thrown */ }
}

// checks that global private browsing is implemented
let isGlobalPBSupported = exports.isGlobalPBSupported = !!pbService && is('Firefox');

// checks that per-window private browsing is implemented
let isWindowPBSupported = exports.isWindowPBSupported =
                          !pbService && !!PrivateBrowsingUtils && is('Firefox');

// checks that per-tab private browsing is implemented
let isTabPBSupported = exports.isTabPBSupported =
                       !pbService && !!PrivateBrowsingUtils && is('Fennec') && satisfiesVersion(version, '>=20.0*');
                       
function ignoreWindow(window) {
  return !isPrivateBrowsingSupported && isWindowPrivate(window) && !isGlobalPBSupported;
}
exports.ignoreWindow = ignoreWindow;

function onChange() {
  // Emit event with in next turn of event loop.
  deferredEmit(exports, pbService.privateBrowsingEnabled ? 'start' : 'stop');
}

// Currently, only Firefox implements the private browsing service.
if (isGlobalPBSupported) {
  // set up an observer for private browsing switches.
  events.on('private-browsing-transition-complete', onChange);
}

// We toggle private browsing mode asynchronously in order to work around
// bug 659629.  Since private browsing transitions are asynchronous
// anyway, this doesn't significantly change the behavior of the API.
let setMode = defer(function setMode(value) {
  value = !!value;  // Cast to boolean.

  // default
  return pbService && (pbService.privateBrowsingEnabled = value);
});
exports.setMode = deprecateFunction(
  setMode,
  'require("private-browsing").activate and require("private-browsing").deactivate ' +
  'is deprecated.'
);

let getMode = function getMode(chromeWin) {
  if (isWindowPrivate(chromeWin))
    return true;

  // default
  return pbService ? pbService.privateBrowsingEnabled : false;
};
exports.getMode = getMode;

exports.on = on.bind(null, exports);

// Make sure listeners are cleaned up.
unload(function() off(exports));
