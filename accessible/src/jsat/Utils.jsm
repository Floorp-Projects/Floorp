/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import('resource://gre/modules/Services.jsm');

var EXPORTED_SYMBOLS = ['Utils', 'Logger'];

var Utils = {
  _buildAppMap: {
    '{3c2e2abc-06d4-11e1-ac3b-374f68613e61}': 'b2g',
    '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}': 'browser',
    '{aa3c5121-dab2-40e2-81ca-7ea25febc110}': 'mobile/android',
    '{a23983c0-fd0e-11dc-95ff-0800200c9a66}': 'mobile/xul'
  },

  get AccRetrieval() {
    if (!this._AccRetrieval) {
      this._AccRetrieval = Cc['@mozilla.org/accessibleRetrieval;1'].
        getService(Ci.nsIAccessibleRetrieval);
    }

    return this._AccRetrieval;
  },

  set MozBuildApp(value) {
    this._buildApp = value;
  },

  get MozBuildApp() {
    if (!this._buildApp)
      this._buildApp = this._buildAppMap[Services.appinfo.ID];
    return this._buildApp;
  },

  get OS() {
    if (!this._OS)
      this._OS = Services.appinfo.OS;
    return this._OS;
  },

  get ScriptName() {
    if (!this._ScriptName)
      this._ScriptName =
        (Services.appinfo.processType == 2) ? 'AccessFuContent' : 'AccessFu';
    return this._ScriptName;
  },

  get AndroidSdkVersion() {
    if (!this._AndroidSdkVersion) {
      if (Services.appinfo.OS == 'Android') {
        this._AndroidSdkVersion = Services.sysinfo.getPropertyAsInt32('version');
      } else {
        // Most useful in desktop debugging.
        this._AndroidSdkVersion = 15;
      }
    }
    return this._AndroidSdkVersion;
  },

  set AndroidSdkVersion(value) {
    // When we want to mimic another version.
    this._AndroidSdkVersion = value;
  },

  getBrowserApp: function getBrowserApp(aWindow) {
    switch (this.MozBuildApp) {
      case 'mobile/android':
        return aWindow.BrowserApp;
      case 'browser':
        return aWindow.gBrowser;
      case 'b2g':
        return aWindow.shell;
      default:
        return null;
    }
  },

  getCurrentBrowser: function getCurrentBrowser(aWindow) {
    if (this.MozBuildApp == 'b2g')
      return this.getBrowserApp(aWindow).contentBrowser;
    return this.getBrowserApp(aWindow).selectedBrowser;
  },

  getCurrentContentDoc: function getCurrentContentDoc(aWindow) {
    return this.getCurrentBrowser(aWindow).contentDocument;
  },

  getMessageManager: function getMessageManager(aBrowser) {
    try {
      return aBrowser.QueryInterface(Ci.nsIFrameLoaderOwner).
         frameLoader.messageManager;
    } catch (x) {
      Logger.error(x);
      return null;
    }
  },

  getAllMessageManagers: function getAllMessageManagers(aWindow) {
    let messageManagers = [];

    for (let i = 0; i < aWindow.messageManager.childCount; i++)
      messageManagers.push(aWindow.messageManager.getChildAt(i));

    let remoteframes = this.getCurrentContentDoc(aWindow).
      querySelectorAll('iframe[remote=true]');

    for (let i = 0; i < remoteframes.length; ++i)
      messageManagers.push(this.getMessageManager(remoteframes[i]));

    Logger.info(messageManagers.length);

    return messageManagers;
  },

  getViewport: function getViewport(aWindow) {
    switch (this.MozBuildApp) {
      case 'mobile/android':
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
  },

  getVirtualCursor: function getVirtualCursor(aDocument) {
    let doc = (aDocument instanceof Ci.nsIAccessible) ? aDocument :
      this.AccRetrieval.getAccessibleFor(aDocument);

    while (doc) {
      try {
        return doc.QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
      } catch (x) {
        doc = doc.parentDocument;
      }
    }

    return null;
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
    dump('[' + Utils.ScriptName + '] ' +
         this._LEVEL_NAMES[aLogLevel] +' ' + message + '\n');
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
      str = '[ ' + Utils.AccRetrieval.getStringRole(aAccessible.role) +
        ' | ' + aAccessible.name + ' ]';
    } catch (x) {
    }

    return str;
  },

  eventToString: function eventToString(aEvent) {
    let str = Utils.AccRetrieval.getStringEventType(aEvent.eventType);
    if (aEvent.eventType == Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE) {
      let event = aEvent.QueryInterface(Ci.nsIAccessibleStateChangeEvent);
      let stateStrings = (event.isExtraState()) ?
        Utils.AccRetrieval.getStringStates(0, event.state) :
        Utils.AccRetrieval.getStringStates(event.state, 0);
      str += ' (' + stateStrings.item(0) + ')';
    }

    return str;
  },

  statesToString: function statesToString(aAccessible) {
    let [state, extState] = Utils.getStates(aAccessible);
    let stringArray = [];
    let stateStrings = Utils.AccRetrieval.getStringStates(state, extState);
    for (var i=0; i < stateStrings.length; i++)
      stringArray.push(stateStrings.item(i));
    return stringArray.join(' ');
  },

  dumpTree: function dumpTree(aLogLevel, aRootAccessible) {
    if (aLogLevel < this.logLevel)
      return;

    this._dumpTreeInternal(aLogLevel, aRootAccessible, 0);
  },

  _dumpTreeInternal: function _dumpTreeInternal(aLogLevel, aAccessible, aIndent) {
    let indentStr = '';
    for (var i=0; i < aIndent; i++)
      indentStr += ' ';
    this.log(aLogLevel, indentStr,
             this.accessibleToString(aAccessible),
             '(' + this.statesToString(aAccessible) + ')');
    for (var i=0; i < aAccessible.childCount; i++)
      this._dumpTreeInternal(aLogLevel, aAccessible.getChildAt(i), aIndent + 1);
    }
};
