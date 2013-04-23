/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/Geometry.jsm');

this.EXPORTED_SYMBOLS = ['Utils', 'Logger', 'PivotContext'];

this.Utils = {
  _buildAppMap: {
    '{3c2e2abc-06d4-11e1-ac3b-374f68613e61}': 'b2g',
    '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}': 'browser',
    '{aa3c5121-dab2-40e2-81ca-7ea25febc110}': 'mobile/android',
    '{a23983c0-fd0e-11dc-95ff-0800200c9a66}': 'mobile/xul'
  },

  init: function Utils_init(aWindow) {
    if (this._win)
      // XXX: only supports attaching to one window now.
      throw new Error('Only one top-level window could used with AccessFu');

    this._win = Cu.getWeakReference(aWindow);
  },

  uninit: function Utils_uninit() {
    if (!this._win) {
      return;
    }
    delete this._win;
  },

  get win() {
    return this._win.get();
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

  get BrowserApp() {
    switch (this.MozBuildApp) {
      case 'mobile/android':
        return this.win.BrowserApp;
      case 'browser':
        return this.win.gBrowser;
      case 'b2g':
        return this.win.shell;
      default:
        return null;
    }
  },

  get CurrentBrowser() {
    if (this.MozBuildApp == 'b2g')
      return this.BrowserApp.contentBrowser;
    return this.BrowserApp.selectedBrowser;
  },

  get CurrentContentDoc() {
    let browser = this.CurrentBrowser;
    return browser ? browser.contentDocument : null;
  },

  get AllMessageManagers() {
    let messageManagers = [];

    for (let i = 0; i < this.win.messageManager.childCount; i++)
      messageManagers.push(this.win.messageManager.getChildAt(i));

    let document = this.CurrentContentDoc;

    if (document) {
      let remoteframes = document.querySelectorAll('iframe[remote=true]');

      for (let i = 0; i < remoteframes.length; ++i)
        messageManagers.push(this.getMessageManager(remoteframes[i]));
    }

    return messageManagers;
  },

  getMessageManager: function getMessageManager(aBrowser) {
    try {
      return aBrowser.QueryInterface(Ci.nsIFrameLoaderOwner).
         frameLoader.messageManager;
    } catch (x) {
      Logger.logException(x);
      return null;
    }
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

this.Logger = {
  DEBUG: 0,
  INFO: 1,
  WARNING: 2,
  ERROR: 3,
  _LEVEL_NAMES: ['DEBUG', 'INFO', 'WARNING', 'ERROR'],

  logLevel: 1, // INFO;

  test: false,

  log: function log(aLogLevel) {
    if (aLogLevel < this.logLevel)
      return;

    let message = Array.prototype.slice.call(arguments, 1).join(' ');
    message = '[' + Utils.ScriptName + '] ' + this._LEVEL_NAMES[aLogLevel] +
      ' ' + message + '\n';
    dump(message);
    // Note: used for testing purposes. If |this.test| is true, also log to
    // the console service.
    if (this.test) {
      try {
        Services.console.logStringMessage(message);
      } catch (ex) {
        // There was an exception logging to the console service.
      }
    }
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

  logException: function logException(aException) {
    try {
      this.error(
        aException.message,
        '(' + aException.fileName + ':' + aException.lineNumber + ')');
    } catch (x) {
      this.error(x);
    }
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
      let stateStrings = event.isExtraState ?
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

/**
 * PivotContext: An object that generates and caches context information
 * for a given accessible and its relationship with another accessible.
 */
this.PivotContext = function PivotContext(aAccessible, aOldAccessible) {
  this._accessible = aAccessible;
  this._oldAccessible =
    this._isDefunct(aOldAccessible) ? null : aOldAccessible;
}

PivotContext.prototype = {
  get accessible() {
    return this._accessible;
  },

  get oldAccessible() {
    return this._oldAccessible;
  },

  /*
   * This is a list of the accessible's ancestry up to the common ancestor
   * of the accessible and the old accessible. It is useful for giving the
   * user context as to where they are in the heirarchy.
   */
  get newAncestry() {
    if (!this._newAncestry) {
      let newLineage = [];
      let oldLineage = [];

      let parent = this._accessible;
      while (parent && (parent = parent.parent))
        newLineage.push(parent);

      parent = this._oldAccessible;
      while (parent && (parent = parent.parent))
        oldLineage.push(parent);

      this._newAncestry = [];

      while (true) {
        let newAncestor = newLineage.pop();
        let oldAncestor = oldLineage.pop();

        if (newAncestor == undefined)
          break;

        if (newAncestor != oldAncestor)
          this._newAncestry.push(newAncestor);
      }

    }

    return this._newAncestry;
  },

  /*
   * Traverse the accessible's subtree in pre or post order.
   * It only includes the accessible's visible chidren.
   */
  _traverse: function _traverse(aAccessible, preorder) {
    let list = [];
    let child = aAccessible.firstChild;
    while (child) {
      let state = {};
      child.getState(state, {});
      if (!(state.value & Ci.nsIAccessibleStates.STATE_INVISIBLE)) {
        let traversed = _traverse(child, preorder);
        // Prepend or append a child, based on traverse order.
        traversed[preorder ? "unshift" : "push"](child);
        list.push.apply(list, traversed);
      }
      child = child.nextSibling;
    }
    return list;
  },

  /*
   * This is a flattened list of the accessible's subtree in preorder.
   * It only includes the accessible's visible chidren.
   */
  get subtreePreorder() {
    if (!this._subtreePreOrder)
      this._subtreePreOrder = this._traverse(this._accessible, true);

    return this._subtreePreOrder;
  },

  /*
   * This is a flattened list of the accessible's subtree in postorder.
   * It only includes the accessible's visible chidren.
   */
  get subtreePostorder() {
    if (!this._subtreePostOrder)
      this._subtreePostOrder = this._traverse(this._accessible, false);

    return this._subtreePostOrder;
  },

  get bounds() {
    if (!this._bounds) {
      let objX = {}, objY = {}, objW = {}, objH = {};

      this._accessible.getBounds(objX, objY, objW, objH);

      // XXX: OOP content provides a screen offset of 0, while in-process provides a real
      // offset. Removing the offset and using content-relative coords normalizes this.
      let docX = {}, docY = {};
      let docRoot = this._accessible.rootDocument.
        QueryInterface(Ci.nsIAccessible);
      docRoot.getBounds(docX, docY, {}, {});

      this._bounds = new Rect(objX.value, objY.value, objW.value, objH.value).
        translate(-docX.value, -docY.value);
    }

    return this._bounds.clone();
  },

  _isDefunct: function _isDefunct(aAccessible) {
    try {
      let extstate = {};
      aAccessible.getState({}, extstate);
      return !!(aAccessible.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT);
    } catch (x) {
      return true;
    }
  }
};