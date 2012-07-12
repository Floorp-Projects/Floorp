/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import('resource://gre/modules/Services.jsm');

var EXPORTED_SYMBOLS = ['Utils', 'Logger'];

var gAccRetrieval = Cc['@mozilla.org/accessibleRetrieval;1'].
  getService(Ci.nsIAccessibleRetrieval);

var Utils = {
  get OS() {
    if (!this._OS)
      this._OS = Services.appinfo.OS;
    return this._OS;
  },

  get AndroidSdkVersion() {
    if (!this._AndroidSdkVersion) {
      let shellVersion = Services.sysinfo.get('shellVersion') || '';
      let matches = shellVersion.match(/\((\d+)\)$/);
      if (matches)
        this._AndroidSdkVersion = parseInt(matches[1]);
      else
        this._AndroidSdkVersion = 15; // Most useful in desktop debugging.
    }
    return this._AndroidSdkVersion;
  },

  set AndroidSdkVersion(value) {
    // When we want to mimic another version.
    this._AndroidSdkVersion = value;
  },

  getBrowserApp: function getBrowserApp(aWindow) {
    switch (this.OS) {
      case 'Android':
        return aWindow.BrowserApp;
      default:
        return aWindow.gBrowser;
    }
  },

  getCurrentContentDoc: function getCurrentContentDoc(aWindow) {
    return this.getBrowserApp(aWindow).selectedBrowser.contentDocument;
  },

  getViewport: function getViewport(aWindow) {
    switch (this.OS) {
      case 'Android':
        return aWindow.BrowserApp.selectedTab.getViewport();
      default:
        return null;
    }
  },

  getStates: function getStates(aAccessible) {
    if (!aAccessible)
      return [0, 0];

    let state = {};
    let extState = {};
    aAccessible.getState(state, extState);
    return [state.value, extState.value];
  }
};

var Logger = {
  DEBUG: 0,
  INFO: 1,
  WARNING: 2,
  ERROR: 3,
  _LEVEL_NAMES: ['DEBUG', 'INFO', 'WARNING', 'ERROR'],

  logLevel: 1, // INFO;

  log: function log(aLogLevel) {
    if (aLogLevel < this.logLevel)
      return;

    let message = Array.prototype.slice.call(arguments, 1).join(' ');
    dump('[AccessFu] ' + this._LEVEL_NAMES[aLogLevel] + ' ' + message + '\n');
  },

  info: function info() {
    this.log.apply(
      this, [this.INFO].concat(Array.prototype.slice.call(arguments)));
  },

  debug: function debug() {
    this.log.apply(
      this, [this.DEBUG].concat(Array.prototype.slice.call(arguments)));
  },

  warning: function warning() {
    this.log.apply(
      this, [this.WARNING].concat(Array.prototype.slice.call(arguments)));
  },

  error: function error() {
    this.log.apply(
      this, [this.ERROR].concat(Array.prototype.slice.call(arguments)));
  },

  accessibleToString: function accessibleToString(aAccessible) {
    let str = '[ defunct ]';
    try {
      str = '[ ' + gAccRetrieval.getStringRole(aAccessible.role) +
        ' | ' + aAccessible.name + ' ]';
    } catch (x) {
    }

    return str;
  },

  eventToString: function eventToString(aEvent) {
    let str = gAccRetrieval.getStringEventType(aEvent.eventType);
    if (aEvent.eventType == Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE) {
      let event = aEvent.QueryInterface(Ci.nsIAccessibleStateChangeEvent);
      let stateStrings = (event.isExtraState()) ?
        gAccRetrieval.getStringStates(0, event.state) :
        gAccRetrieval.getStringStates(event.state, 0);
      str += ' (' + stateStrings.item(0) + ')';
    }

    return str;
  }
};
