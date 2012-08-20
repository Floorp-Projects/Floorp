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
  _buildAppMap: {
    '{3c2e2abc-06d4-11e1-ac3b-374f68613e61}': 'b2g',
    '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}': 'browser',
    '{aa3c5121-dab2-40e2-81ca-7ea25febc110}': 'mobile/android',
    '{a23983c0-fd0e-11dc-95ff-0800200c9a66}': 'mobile/xul'
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

  getCurrentContentDoc: function getCurrentContentDoc(aWindow) {
    if (this.MozBuildApp == "b2g")
      return this.getBrowserApp(aWindow).contentBrowser.contentDocument;
    return this.getBrowserApp(aWindow).selectedBrowser.contentDocument;
  },

  getAllDocuments: function getAllDocuments(aWindow) {
    let doc = gAccRetrieval.
      getAccessibleFor(this.getCurrentContentDoc(aWindow)).
      QueryInterface(Ci.nsIAccessibleDocument);
    let docs = [];
    function getAllDocuments(aDocument) {
      docs.push(aDocument.DOMDocument);
      for (let i = 0; i < aDocument.childDocumentCount; i++)
        getAllDocuments(aDocument.getChildDocumentAt(i));
    }
    getAllDocuments(doc);
    return docs;
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
      gAccRetrieval.getAccessibleFor(aDocument);

    while (doc) {
      try {
        return doc.QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
      } catch (x) {
        doc = doc.parentDocument;
      }
    }

    return null;
  },

  scroll: function scroll(aWindow, aPage, aHorizontal) {
    for each (let doc in this.getAllDocuments(aWindow)) {
      // First see if we could scroll a window.
      let win = doc.defaultView;
      if (!aHorizontal && win.scrollMaxY &&
          ((aPage > 0 && win.scrollY < win.scrollMaxY) ||
           (aPage < 0 && win.scrollY > 0))) {
        win.scroll(0, win.innerHeight);
        return true;
      } else if (aHorizontal && win.scrollMaxX &&
                 ((aPage > 0 && win.scrollX < win.scrollMaxX) ||
                  (aPage < 0 && win.scrollX > 0))) {
        win.scroll(win.innerWidth, 0);
        return true;
      }

      // Second, try to scroll main section or current target if there is no
      // main section.
      let main = doc.querySelector('[role=main]') ||
        doc.querySelector(':target');

      if (main) {
        if ((!aHorizontal && main.clientHeight < main.scrollHeight) ||
          (aHorizontal && main.clientWidth < main.scrollWidth)) {
          let s = win.getComputedStyle(main);
          if (!aHorizontal) {
            if (s.overflowY == 'scroll' || s.overflowY == 'auto') {
              main.scrollTop += aPage * main.clientHeight;
              return true;
            }
          } else {
            if (s.overflowX == 'scroll' || s.overflowX == 'auto') {
              main.scrollLeft += aPage * main.clientWidth;
              return true;
            }
          }
        }
      }
    }

    return false;
  },

  changePage: function changePage(aWindow, aPage) {
    for each (let doc in this.getAllDocuments(aWindow)) {
      // Get current main section or active target.
      let main = doc.querySelector('[role=main]') ||
        doc.querySelector(':target');
      if (!main)
        continue;

      let mainAcc = gAccRetrieval.getAccessibleFor(main);
      if (!mainAcc)
        continue;

      let controllers = mainAcc.
        getRelationByType(Ci.nsIAccessibleRelation.RELATION_CONTROLLED_BY);

      for (var i=0; controllers.targetsCount > i; i++) {
        let controller = controllers.getTarget(i);
        // If the section has a controlling slider, it should be considered
        // the page-turner.
        if (controller.role == Ci.nsIAccessibleRole.ROLE_SLIDER) {
          // Sliders are controlled with ctrl+right/left. I just decided :)
          let evt = doc.createEvent("KeyboardEvent");
          evt.initKeyEvent('keypress', true, true, null,
                           true, false, false, false,
                           (aPage > 0) ? evt.DOM_VK_RIGHT : evt.DOM_VK_LEFT, 0);
          controller.DOMNode.dispatchEvent(evt);
          return true;
        }
      }
    }

    return false;
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
  },

  statesToString: function statesToString(aAccessible) {
    let [state, extState] = Utils.getStates(aAccessible);
    let stringArray = [];
    let stateStrings = gAccRetrieval.getStringStates(state, extState);
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
