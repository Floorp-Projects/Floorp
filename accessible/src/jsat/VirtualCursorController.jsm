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
  NOT_EDITABLE: 0,
  SINGLE_LINE_EDITABLE: 1,
  MULTI_LINE_EDITABLE: 2,

  attach: function attach(aWindow) {
    this.chromeWin = aWindow;
    this.chromeWin.document.addEventListener('keypress', this, true);
  },

  detach: function detach() {
    this.chromeWin.document.removeEventListener('keypress', this, true);
  },

  _getBrowserApp: function _getBrowserApp() {
    switch (Services.appinfo.OS) {
      case 'Android':
        return this.chromeWin.BrowserApp;
      default:
        return this.chromeWin.gBrowser;
    }
  },

  handleEvent: function handleEvent(aEvent) {
    let document = this._getBrowserApp().selectedBrowser.contentDocument;
    let target = aEvent.target;

    switch (aEvent.keyCode) {
      case aEvent.DOM_VK_END:
        this.moveForward(document, true);
        break;
      case aEvent.DOM_VK_HOME:
        this.moveBackward(document, true);
        break;
      case aEvent.DOM_VK_RIGHT:
        if (this._isEditableText(target) &&
            target.selectionEnd != target.textLength)
          // Don't move forward if caret is not at end of entry.
          // XXX: Fix for rtl
          return;
        this.moveForward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_LEFT:
        if (this._isEditableText(target) &&
            target.selectionEnd != 0)
          // Don't move backward if caret is not at start of entry.
          // XXX: Fix for rtl
          return;
        this.moveBackward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_UP:
        if (this._isEditableText(target) == this.MULTI_LINE_EDITABLE &&
            target.selectionEnd != 0)
          // Don't blur content if caret is not at start of text area.
          return;
        if (Services.appinfo.OS == 'Android')
          // Return focus to native Android browser chrome.
          Cc['@mozilla.org/android/bridge;1'].
            getService(Ci.nsIAndroidBridge).handleGeckoMessage(
              JSON.stringify({ gecko: { type: 'ToggleChrome:Focus' } }));
        break;
      case aEvent.DOM_VK_RETURN:
      case aEvent.DOM_VK_ENTER:
        if (this._isEditableText(target))
          return;
        this.activateCurrent(document);
        break;
      default:
        return;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  _isEditableText: function _isEditableText(aElement) {
    // XXX: Support contentEditable and design mode
    if (aElement instanceof Ci.nsIDOMHTMLInputElement &&
        aElement.mozIsTextField(false))
      return this.SINGLE_LINE_EDITABLE;

    if (aElement instanceof Ci.nsIDOMHTMLTextAreaElement)
      return this.MULTI_LINE_EDITABLE;

    return this.NOT_EDITABLE;
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
    getMatchRoles: function SimpleTraversalRule_getmatchRoles(aRules) {
      aRules.value = this._matchRoles;
      return this._matchRoles.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function SimpleTraversalRule_match(aAccessible) {
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
