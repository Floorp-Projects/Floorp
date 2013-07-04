/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const EVENT_STATE_CHANGE = Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE;

const ROLE_CELL = Ci.nsIAccessibleRole.ROLE_CELL;
const ROLE_COLUMNHEADER = Ci.nsIAccessibleRole.ROLE_COLUMNHEADER;
const ROLE_ROWHEADER = Ci.nsIAccessibleRole.ROLE_ROWHEADER;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, 'Services',
  'resource://gre/modules/Services.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Rect',
  'resource://gre/modules/Geometry.jsm');

this.EXPORTED_SYMBOLS = ['Utils', 'Logger', 'PivotContext', 'PrefCache'];

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
    if (!this._win) {
      return null;
    }
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

  get widgetToolkit() {
    if (!this._widgetToolkit)
      this._widgetToolkit = Services.appinfo.widgetToolkit;
    return this._widgetToolkit;
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
    if (!this.win) {
      return null;
    }
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
    if (!this.BrowserApp) {
      return null;
    }
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
      let remoteframes = document.querySelectorAll('iframe');

      for (let i = 0; i < remoteframes.length; ++i) {
        let mm = this.getMessageManager(remoteframes[i]);
        if (mm) {
          messageManagers.push(mm);
        }
      }

    }

    return messageManagers;
  },

  get isContentProcess() {
    delete this.isContentProcess;
    this.isContentProcess =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;
    return this.isContentProcess;
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

  getAttributes: function getAttributes(aAccessible) {
    let attributes = {};

    if (aAccessible && aAccessible.attributes) {
      let attributesEnum = aAccessible.attributes.enumerate();

      // Populate |attributes| object with |aAccessible|'s attribute key-value
      // pairs.
      while (attributesEnum.hasMoreElements()) {
        let attribute = attributesEnum.getNext().QueryInterface(
          Ci.nsIPropertyElement);
        attributes[attribute.key] = attribute.value;
      }
    }

    return attributes;
  },

  getVirtualCursor: function getVirtualCursor(aDocument) {
    let doc = (aDocument instanceof Ci.nsIAccessible) ? aDocument :
      this.AccRetrieval.getAccessibleFor(aDocument);

    return doc.QueryInterface(Ci.nsIAccessibleDocument).virtualCursor;
  },

  getPixelsPerCSSPixel: function getPixelsPerCSSPixel(aWindow) {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).screenPixelsPerCSSPixel;
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

  logException: function logException(
    aException, aErrorMessage = 'An exception has occured') {
    try {
      let stackMessage = '';
      if (aException.stack) {
        stackMessage = '  ' + aException.stack.replace(/\n/g, '\n  ');
      } else if (aException.location) {
        let frame = aException.location;
        let stackLines = [];
        while (frame && frame.lineNumber) {
          stackLines.push(
            '  ' + frame.name + '@' + frame.filename + ':' + frame.lineNumber);
          frame = frame.caller;
        }
        stackMessage = stackLines.join('\n');
      } else {
        stackMessage = '(' + aException.fileName + ':' + aException.lineNumber + ')';
      }
      this.error(aErrorMessage + ':\n ' +
                 aException.message + '\n' +
                 stackMessage);
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
    if (aEvent.eventType == EVENT_STATE_CHANGE) {
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

  /**
   * Get a list of |aAccessible|'s ancestry up to the root.
   * @param  {nsIAccessible} aAccessible.
   * @return {Array} Ancestry list.
   */
  _getAncestry: function _getAncestry(aAccessible) {
    let ancestry = [];
    let parent = aAccessible;
    while (parent && (parent = parent.parent)) {
      ancestry.push(parent);
    }
    return ancestry.reverse();
  },

  /**
   * A list of the old accessible's ancestry.
   */
  get oldAncestry() {
    if (!this._oldAncestry) {
      if (!this._oldAccessible) {
        this._oldAncestry = [];
      } else {
        this._oldAncestry = this._getAncestry(this._oldAccessible);
        this._oldAncestry.push(this._oldAccessible);
      }
    }
    return this._oldAncestry;
  },

  /**
   * A list of the current accessible's ancestry.
   */
  get currentAncestry() {
    if (!this._currentAncestry) {
      this._currentAncestry = this._getAncestry(this._accessible);
    }
    return this._currentAncestry;
  },

  /*
   * This is a list of the accessible's ancestry up to the common ancestor
   * of the accessible and the old accessible. It is useful for giving the
   * user context as to where they are in the heirarchy.
   */
  get newAncestry() {
    if (!this._newAncestry) {
      this._newAncestry = [currentAncestor for (
        [index, currentAncestor] of Iterator(this.currentAncestry)) if (
          currentAncestor !== this.oldAncestry[index])];
    }
    return this._newAncestry;
  },

  /*
   * Traverse the accessible's subtree in pre or post order.
   * It only includes the accessible's visible chidren.
   * Note: needSubtree is a function argument that can be used to determine
   * whether aAccessible's subtree is required.
   */
  _traverse: function _traverse(aAccessible, aPreorder, aStop) {
    if (aStop && aStop(aAccessible)) {
      return;
    }
    let child = aAccessible.firstChild;
    while (child) {
      let state = {};
      child.getState(state, {});
      if (!(state.value & Ci.nsIAccessibleStates.STATE_INVISIBLE)) {
        if (aPreorder) {
          yield child;
          [yield node for (node of this._traverse(child, aPreorder, aStop))];
        } else {
          [yield node for (node of this._traverse(child, aPreorder, aStop))];
          yield child;
        }
      }
      child = child.nextSibling;
    }
  },

  /*
   * A subtree generator function, used to generate a flattened
   * list of the accessible's subtree in pre or post order.
   * It only includes the accessible's visible chidren.
   * @param {boolean} aPreorder A flag for traversal order. If true, traverse
   * in preorder; if false, traverse in postorder.
   * @param {function} aStop An optional function, indicating whether subtree
   * traversal should stop.
   */
  subtreeGenerator: function subtreeGenerator(aPreorder, aStop) {
    return this._traverse(this._accessible, aPreorder, aStop);
  },

  getCellInfo: function getCellInfo(aAccessible) {
    if (!this._cells) {
      this._cells = new WeakMap();
    }

    let domNode = aAccessible.DOMNode;
    if (this._cells.has(domNode)) {
      return this._cells.get(domNode);
    }

    let cellInfo = {};
    let getAccessibleCell = function getAccessibleCell(aAccessible) {
      if (!aAccessible) {
        return null;
      }
      if ([ROLE_CELL, ROLE_COLUMNHEADER, ROLE_ROWHEADER].indexOf(
        aAccessible.role) < 0) {
          return null;
      }
      try {
        return aAccessible.QueryInterface(Ci.nsIAccessibleTableCell);
      } catch (x) {
        Logger.logException(x);
        return null;
      }
    };
    let getHeaders = function getHeaders(aHeaderCells) {
      let enumerator = aHeaderCells.enumerate();
      while (enumerator.hasMoreElements()) {
        yield enumerator.getNext().QueryInterface(Ci.nsIAccessible).name;
      }
    };

    cellInfo.current = getAccessibleCell(aAccessible);

    if (!cellInfo.current) {
      Logger.warning(aAccessible,
        'does not support nsIAccessibleTableCell interface.');
      this._cells.set(domNode, null);
      return null;
    }

    let table = cellInfo.current.table;
    if (table.isProbablyForLayout()) {
      this._cells.set(domNode, null);
      return null;
    }

    cellInfo.previous = null;
    let oldAncestry = this.oldAncestry.reverse();
    let ancestor = oldAncestry.shift();
    while (!cellInfo.previous && ancestor) {
      let cell = getAccessibleCell(ancestor);
      if (cell && cell.table === table) {
        cellInfo.previous = cell;
      }
      ancestor = oldAncestry.shift();
    }

    if (cellInfo.previous) {
      cellInfo.rowChanged = cellInfo.current.rowIndex !==
        cellInfo.previous.rowIndex;
      cellInfo.columnChanged = cellInfo.current.columnIndex !==
        cellInfo.previous.columnIndex;
    } else {
      cellInfo.rowChanged = true;
      cellInfo.columnChanged = true;
    }

    cellInfo.rowExtent = cellInfo.current.rowExtent;
    cellInfo.columnExtent = cellInfo.current.columnExtent;
    cellInfo.columnIndex = cellInfo.current.columnIndex;
    cellInfo.rowIndex = cellInfo.current.rowIndex;

    cellInfo.columnHeaders = [];
    if (cellInfo.columnChanged && cellInfo.current.role !==
      ROLE_COLUMNHEADER) {
      cellInfo.columnHeaders = [headers for (headers of getHeaders(
        cellInfo.current.columnHeaderCells))];
    }
    cellInfo.rowHeaders = [];
    if (cellInfo.rowChanged && cellInfo.current.role === ROLE_CELL) {
      cellInfo.rowHeaders = [headers for (headers of getHeaders(
        cellInfo.current.rowHeaderCells))];
    }

    this._cells.set(domNode, cellInfo);
    return cellInfo;
  },

  get bounds() {
    if (!this._bounds) {
      let objX = {}, objY = {}, objW = {}, objH = {};

      this._accessible.getBounds(objX, objY, objW, objH);

      this._bounds = new Rect(objX.value, objY.value, objW.value, objH.value);
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

this.PrefCache = function PrefCache(aName, aCallback, aRunCallbackNow) {
  this.name = aName;
  this.callback = aCallback;

  let branch = Services.prefs;
  this.value = this._getValue(branch);

  if (this.callback && aRunCallbackNow) {
    try {
      this.callback(this.name, this.value);
    } catch (x) {
      Logger.logException(x);
    }
  }

  branch.addObserver(aName, this, true);
};

PrefCache.prototype = {
  _getValue: function _getValue(aBranch) {
    if (!this.type) {
      this.type = aBranch.getPrefType(this.name);
    }

    switch (this.type) {
      case Ci.nsIPrefBranch.PREF_STRING:
        return aBranch.getCharPref(this.name);
      case Ci.nsIPrefBranch.PREF_INT:
        return aBranch.getIntPref(this.name);
      case Ci.nsIPrefBranch.PREF_BOOL:
        return aBranch.getBoolPref(this.name);
      default:
        return null;
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    this.value = this._getValue(aSubject.QueryInterface(Ci.nsIPrefBranch));
    if (this.callback) {
      try {
        this.callback(this.name, this.value);
      } catch (x) {
        Logger.logException(x);
      }
    }
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference])
};