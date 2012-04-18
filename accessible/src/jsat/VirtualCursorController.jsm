/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

var EXPORTED_SYMBOLS = ['VirtualCursorController'];

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

var gAccRetrieval = Cc['@mozilla.org/accessibleRetrieval;1'].
  getService(Ci.nsIAccessibleRetrieval);

var VirtualCursorController = {
  attach: function attach(aWindow) {
    this.chromeWin = aWindow;
    this.chromeWin.document.addEventListener('keypress', this.onkeypress, true);
  },

  detach: function detach() {
    this.chromeWin.document.removeEventListener('keypress', this.onkeypress);
  },

  getBrowserApp: function getBrowserApp() {
    switch (Services.appinfo.OS) {
      case 'Android':
        return this.chromeWin.BrowserApp;
      default:
        return this.chromeWin.gBrowser;
    }
  },

  onkeypress: function onkeypress(aEvent) {
    let document = VirtualCursorController.getBrowserApp().
      selectedBrowser.contentDocument;

    dump('keypress ' + aEvent.keyCode + '\n');

    switch (aEvent.keyCode) {
      case aEvent.DOM_END:
        VirtualCursorController.moveForward(document, true);
        break;
      case aEvent.DOM_HOME:
        VirtualCursorController.moveBackward(document, true);
        break;
      case aEvent.DOM_VK_DOWN:
        VirtualCursorController.moveForward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_UP:
        VirtualCursorController.moveBackward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_RETURN:
        //It is true that desktop does not map the kp enter key to ENTER.
        //So for desktop we require a ctrl+return instead.
        if (Services.appinfo.OS == 'Android' || !aEvent.ctrlKey)
          return;
      case aEvent.DOM_VK_ENTER:
        VirtualCursorController.activateCurrent(document);
        break;
      default:
        return;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  moveForward: function moveForward(document, last) {
    let virtualCursor = this.getVirtualCursor(document);
    if (last) {
      virtualCursor.moveLast(this.SimpleTraversalRule);
    } else {
      virtualCursor.moveNext(this.SimpleTraversalRule);
    }
  },

  moveBackward: function moveBackward(document, first) {
    let virtualCursor = this.getVirtualCursor(document);

    if (first) {
      virtualCursor.moveFirst(this.SimpleTraversalRule);
      return

    }

    if (!virtualCursor.movePrevious(this.SimpleTraversalRule) &&
        Services.appinfo.OS == 'Android') {
      // Return focus to browser chrome, which in Android is a native widget.
      Cc['@mozilla.org/android/bridge;1'].
        getService(Ci.nsIAndroidBridge).handleGeckoMessage(
          JSON.stringify({ gecko: { type: 'ToggleChrome:Focus' } }));
      virtualCursor.position = null;
    }
  },

  activateCurrent: function activateCurrent(document) {
    let virtualCursor = this.getVirtualCursor(document);
    let acc = virtualCursor.position;

    if (acc.numActions > 0)
      acc.doAction(0);
  },

  getVirtualCursor: function getVirtualCursor(document) {
    return gAccRetrieval.getAccessibleFor(document).
      QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
  },

  SimpleTraversalRule: {
    getMatchRoles: function(aRules) {
      aRules.value = [];
      return 0;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function(aAccessible) {
      let rv = Ci.nsIAccessibleTraversalRule.FILTER_IGNORE;
      if (aAccessible.childCount == 0) {
        // TODO: Find a better solution for ROLE_STATICTEXT.
        // Right now it helps filter list bullets, but it is also used
        // in CSS generated content.
        let ignoreRoles = [Ci.nsIAccessibleRole.ROLE_WHITESPACE,
                           Ci.nsIAccessibleRole.ROLE_STATICTEXT];
        let state = {};
        aAccessible.getState(state, {});
        if ((state.value & Ci.nsIAccessibleStates.STATE_FOCUSABLE) ||
          (aAccessible.name && ignoreRoles.indexOf(aAccessible.role) < 0))
          rv = Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
        }
      return rv;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  }
};
