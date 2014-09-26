/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci, Cu } = require('chrome');
const { is } = require('../system/xul-app');
const { isWindowPrivate } = require('../window/utils');
const { isPrivateBrowsingSupported } = require('../self');
const { dispatcher } = require("../util/dispatcher");

let PrivateBrowsingUtils;

// Private browsing is only supported in Fx
try {
  PrivateBrowsingUtils = Cu.import('resource://gre/modules/PrivateBrowsingUtils.jsm', {}).PrivateBrowsingUtils;
}
catch (e) {}

exports.isGlobalPBSupported = false;

// checks that per-window private browsing is implemented
let isWindowPBSupported = exports.isWindowPBSupported =
                          !!PrivateBrowsingUtils && is('Firefox');

// checks that per-tab private browsing is implemented
let isTabPBSupported = exports.isTabPBSupported =
                       !!PrivateBrowsingUtils && is('Fennec');

function isPermanentPrivateBrowsing() {
 return !!(PrivateBrowsingUtils && PrivateBrowsingUtils.permanentPrivateBrowsing);
}
exports.isPermanentPrivateBrowsing = isPermanentPrivateBrowsing;

function ignoreWindow(window) {
  return !isPrivateBrowsingSupported && isWindowPrivate(window);
}
exports.ignoreWindow = ignoreWindow;

let getMode = function getMode(chromeWin) {
  return (chromeWin !== undefined && isWindowPrivate(chromeWin));
};
exports.getMode = getMode;

const isPrivate = dispatcher("isPrivate");
isPrivate.when(isPermanentPrivateBrowsing, _ => true);
isPrivate.when(x => x instanceof Ci.nsIDOMWindow, isWindowPrivate);
isPrivate.when(x => Ci.nsIPrivateBrowsingChannel && x instanceof Ci.nsIPrivateBrowsingChannel, x => x.isChannelPrivate);
isPrivate.define(() => false);
exports.isPrivate = isPrivate;
