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
    this.chromeWin.document.removeEventListener('keypress', this.onkeypress, true);
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
      case aEvent.DOM_VK_END:
        VirtualCursorController.moveForward(document, true);
        break;
      case aEvent.DOM_VK_HOME:
        VirtualCursorController.moveBackward(document, true);
        break;
      case aEvent.DOM_VK_RIGHT:
        VirtualCursorController.moveForward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_LEFT:
        VirtualCursorController.moveBackward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_UP:
        if (Services.appinfo.OS == 'Android')
          // Return focus to browser chrome, which in Android is a native widget.
          Cc['@mozilla.org/android/bridge;1'].
            getService(Ci.nsIAndroidBridge).handleGeckoMessage(
              JSON.stringify({ gecko: { type: 'ToggleChrome:Focus' } }));
        break;
      case aEvent.DOM_VK_RETURN:
        // XXX: It is true that desktop does not map the keypad enter key to
        // DOM_VK_ENTER. So for desktop we require a ctrl+return instead.
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
    } else {
      virtualCursor.movePrevious(this.SimpleTraversalRule);
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
      aRules.value = this._matchRoles;
      return this._matchRoles.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function(aAccessible) {
      switch (aAccessible.role) {
      case Ci.nsIAccessibleRole.ROLE_COMBOBOX:
        // We don't want to ignore the subtree because this is often
        // where the list box hangs out.
        return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
      case Ci.nsIAccessibleRole.ROLE_TEXT_LEAF:
        {
          // Nameless text leaves are boring, skip them.
          let name = aAccessible.name;
          if (name && name.trim())
            return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
          else
            return Ci.nsIAccessibleTraversalRule.FILTER_IGNORE;
        }
      case Ci.nsIAccessibleRole.ROLE_LINK:
        // If the link has children we should land on them instead.
        // Image map links don't have children so we need to match those.
        if (aAccessible.childCount == 0)
          return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
        else
          return Ci.nsIAccessibleTraversalRule.FILTER_IGNORE;
      default:
        // Ignore the subtree, if there is one. So that we don't land on
        // the same content that was already presented by its parent.
        return Ci.nsIAccessibleTraversalRule.FILTER_MATCH |
          Ci.nsIAccessibleTraversalRule.FILTER_IGNORE_SUBTREE;
      }
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule]),

    _matchRoles: [
      Ci.nsIAccessibleRole.ROLE_MENUITEM,
      Ci.nsIAccessibleRole.ROLE_LINK,
      Ci.nsIAccessibleRole.ROLE_PAGETAB,
      Ci.nsIAccessibleRole.ROLE_GRAPHIC,
      // XXX: Find a better solution for ROLE_STATICTEXT.
      // It allows to filter list bullets but at the same time it
      // filters CSS generated content too as an unwanted side effect.
      // Ci.nsIAccessibleRole.ROLE_STATICTEXT,
      Ci.nsIAccessibleRole.ROLE_TEXT_LEAF,
      Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
      Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
      Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
      Ci.nsIAccessibleRole.ROLE_COMBOBOX,
      Ci.nsIAccessibleRole.ROLE_PROGRESSBAR,
      Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWN,
      Ci.nsIAccessibleRole.ROLE_BUTTONMENU,
      Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM,
      Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT,
      Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM,
      Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
      Ci.nsIAccessibleRole.ROLE_ENTRY
    ]
  }
};
