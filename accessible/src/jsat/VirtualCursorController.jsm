/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

var EXPORTED_SYMBOLS = ['VirtualCursorController'];

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

var gAccRetrieval = Cc['@mozilla.org/accessibleRetrieval;1'].
  getService(Ci.nsIAccessibleRetrieval);

function BaseTraversalRule(aRoles, aMatchFunc) {
  this._matchRoles = aRoles;
  this._matchFunc = aMatchFunc;
}

BaseTraversalRule.prototype = {
    getMatchRoles: function BaseTraversalRule_getmatchRoles(aRules) {
      aRules.value = this._matchRoles;
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
    Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function BaseTraversalRule_match(aAccessible)
    {
      if (this._matchFunc)
        return this._matchFunc(aAccessible);

      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
};

var TraversalRules = {
  Simple: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_MENUITEM,
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
     Ci.nsIAccessibleRole.ROLE_ENTRY],
    function Simple_match(aAccessible) {
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
    }
  ),

  Anchor: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_LINK],
    function Anchor_match(aAccessible)
    {
      // We want to ignore links, only focus named anchors.
      let state = {};
      let extraState = {};
      aAccessible.getState(state, extraState);
      if (state.value & Ci.nsIAccessibleStates.STATE_LINKED) {
        return Ci.nsIAccessibleTraversalRule.FILTER_IGNORE;
      } else {
        return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
      }
    }),

  Button: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
     Ci.nsIAccessibleRole.ROLE_SPINBUTTON,
     Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
     Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWN,
     Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWNGRID]),

  Combobox: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_COMBOBOX,
     Ci.nsIAccessibleRole.ROLE_LISTBOX]),

  Entry: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_ENTRY,
     Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT]),

  FormElement: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
     Ci.nsIAccessibleRole.ROLE_SPINBUTTON,
     Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
     Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWN,
     Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWNGRID,
     Ci.nsIAccessibleRole.ROLE_COMBOBOX,
     Ci.nsIAccessibleRole.ROLE_LISTBOX,
     Ci.nsIAccessibleRole.ROLE_ENTRY,
     Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT,
     Ci.nsIAccessibleRole.ROLE_PAGETAB,
     Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
     Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM,
     Ci.nsIAccessibleRole.ROLE_SLIDER,
     Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
     Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM]),

  Graphic: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_GRAPHIC]),

  Heading: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_HEADING]),

  ListItem: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_LISTITEM,
     Ci.nsIAccessibleRole.ROLE_TERM]),

  Link: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_LINK],
    function Link_match(aAccessible)
    {
      // We want to ignore anchors, only focus real links.
      let state = {};
      let extraState = {};
      aAccessible.getState(state, extraState);
      if (state.value & Ci.nsIAccessibleStates.STATE_LINKED) {
        return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
      } else {
        return Ci.nsIAccessibleTraversalRule.FILTER_IGNORE;
      }
    }),

  List: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_LIST,
     Ci.nsIAccessibleRole.ROLE_DEFINITION_LIST]),

  PageTab: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_PAGETAB]),

  RadioButton: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
     Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM]),

  Separator: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_SEPARATOR]),

  Table: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_TABLE]),

  Checkbox: new BaseTraversalRule(
    [Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
     Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM])
};

var VirtualCursorController = {
  exploreByTouch: false,
  editableState: 0,

  attach: function attach(aWindow) {
    this.chromeWin = aWindow;
    this.chromeWin.document.addEventListener('keypress', this, true);
    this.chromeWin.document.addEventListener('mousemove', this, true);
  },

  detach: function detach() {
    this.chromeWin.document.removeEventListener('keypress', this, true);
    this.chromeWin.document.removeEventListener('mousemove', this, true);
  },

  handleEvent: function VirtualCursorController_handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'keypress':
        this._handleKeypress(aEvent);
        break;
      case 'mousemove':
        this._handleMousemove(aEvent);
        break;
    }
  },

  _handleMousemove: function _handleMousemove(aEvent) {
    // Explore by touch is disabled.
    if (!this.exploreByTouch)
      return;

    // On non-Android we use the shift key to simulate touch.
    if (Utils.OS != 'Android' && !aEvent.shiftKey)
      return;

    // We should not be calling moveToPoint more than 10 times a second.
    // It is granular enough to feel natural, and it does not hammer the CPU.
    if (!this._handleMousemove._lastEventTime ||
        aEvent.timeStamp - this._handleMousemove._lastEventTime >= 100) {
      this.moveToPoint(Utils.getCurrentContentDoc(this.chromeWin),
                       aEvent.screenX, aEvent.screenY);
      this._handleMousemove._lastEventTime = aEvent.timeStamp;
    }

    aEvent.preventDefault();
    aEvent.stopImmediatePropagation();
  },

  _handleKeypress: function _handleKeypress(aEvent) {
    let document = Utils.getCurrentContentDoc(this.chromeWin);
    let target = aEvent.target;

    switch (aEvent.keyCode) {
      case 0:
        // an alphanumeric key was pressed, handle it separately.
        // If it was pressed with either alt or ctrl, just pass through.
        // If it was pressed with meta, pass the key on without the meta.
        if (this.editableState ||
            aEvent.ctrlKey || aEvent.altKey || aEvent.metaKey)
          return;

        let key = String.fromCharCode(aEvent.charCode);
        let methodName = '', rule = {};
        try {
          [methodName, rule] = this.keyMap[key];
        } catch (x) {
          return;
        }
        this[methodName](document, false, rule);
        break;
      case aEvent.DOM_VK_END:
        if (this.editableState) {
          if (target.selectionEnd != target.textLength)
            // Don't move forward if caret is not at end of entry.
            // XXX: Fix for rtl
            return;
          else
            target.blur();
        }
        this.moveForward(document, true);
        break;
      case aEvent.DOM_VK_HOME:
        if (this.editableState) {
          if (target.selectionEnd != 0)
            // Don't move backward if caret is not at start of entry.
            // XXX: Fix for rtl
            return;
          else
            target.blur();
        }
        this.moveBackward(document, true);
        break;
      case aEvent.DOM_VK_RIGHT:
        if (this.editableState) {
          if (target.selectionEnd != target.textLength)
            // Don't move forward if caret is not at end of entry.
            // XXX: Fix for rtl
            return;
          else
            target.blur();
        }
        this.moveForward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_LEFT:
        if (this.editableState) {
          if (target.selectionEnd != 0)
            // Don't move backward if caret is not at start of entry.
            // XXX: Fix for rtl
            return;
          else
            target.blur();
        }
        this.moveBackward(document, aEvent.shiftKey);
        break;
      case aEvent.DOM_VK_UP:
        if (this.editableState & Ci.nsIAccessibleStates.EXT_STATE_MULTI_LINE) {
          if (target.selectionEnd != 0)
            // Don't blur content if caret is not at start of text area.
            return;
          else
            target.blur();
        }

        if (Utils.OS == 'Android')
          // Return focus to native Android browser chrome.
          Cc['@mozilla.org/android/bridge;1'].
            getService(Ci.nsIAndroidBridge).handleGeckoMessage(
              JSON.stringify({ gecko: { type: 'ToggleChrome:Focus' } }));
        break;
      case aEvent.DOM_VK_RETURN:
      case aEvent.DOM_VK_ENTER:
        if (this.editableState)
          return;
        this.activateCurrent(document);
        break;
      default:
        return;
    }

    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  moveToPoint: function moveToPoint(aDocument, aX, aY) {
    Utils.getVirtualCursor(aDocument).moveToPoint(TraversalRules.Simple,
                                                  aX, aY, true);
  },

  moveForward: function moveForward(aDocument, aLast, aRule) {
    let virtualCursor = Utils.getVirtualCursor(aDocument);
    if (aLast) {
      virtualCursor.moveLast(TraversalRules.Simple);
    } else {
      try {
        virtualCursor.moveNext(aRule || TraversalRules.Simple);
      } catch (x) {
        this.moveCursorToObject(
          virtualCursor,
          gAccRetrieval.getAccessibleFor(aDocument.activeElement), aRule);
      }
    }
  },

  moveBackward: function moveBackward(aDocument, aFirst, aRule) {
    let virtualCursor = Utils.getVirtualCursor(aDocument);
    if (aFirst) {
      virtualCursor.moveFirst(TraversalRules.Simple);
    } else {
      try {
        virtualCursor.movePrevious(aRule || TraversalRules.Simple);
      } catch (x) {
        this.moveCursorToObject(
          virtualCursor,
          gAccRetrieval.getAccessibleFor(aDocument.activeElement), aRule);
      }
    }
  },

  activateCurrent: function activateCurrent(document) {
    let virtualCursor = Utils.getVirtualCursor(document);
    let acc = virtualCursor.position;

    if (acc.actionCount > 0) {
      acc.doAction(0);
    } else {
      // XXX Some mobile widget sets do not expose actions properly
      // (via ARIA roles, etc.), so we need to generate a click.
      // Could possibly be made simpler in the future. Maybe core
      // engine could expose nsCoreUtiles::DispatchMouseEvent()?
      let docAcc = gAccRetrieval.getAccessibleFor(this.chromeWin.document);
      let docX = {}, docY = {}, docW = {}, docH = {};
      docAcc.getBounds(docX, docY, docW, docH);

      let objX = {}, objY = {}, objW = {}, objH = {};
      acc.getBounds(objX, objY, objW, objH);

      let x = Math.round((objX.value - docX.value) + objW.value / 2);
      let y = Math.round((objY.value - docY.value) + objH.value / 2);

      let cwu = this.chromeWin.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils);
      cwu.sendMouseEventToWindow('mousedown', x, y, 0, 1, 0, false);
      cwu.sendMouseEventToWindow('mouseup', x, y, 0, 1, 0, false);
    }
  },

  moveCursorToObject: function moveCursorToObject(aVirtualCursor,
                                                  aAccessible, aRule) {
    aVirtualCursor.moveNext(aRule || TraversalRules.Simple, aAccessible, true);
  },

  keyMap: {
    a: ['moveForward', TraversalRules.Anchor],
    A: ['moveBackward', TraversalRules.Anchor],
    b: ['moveForward', TraversalRules.Button],
    B: ['moveBackward', TraversalRules.Button],
    c: ['moveForward', TraversalRules.Combobox],
    C: ['moveBackward', TraversalRules.Combobox],
    e: ['moveForward', TraversalRules.Entry],
    E: ['moveBackward', TraversalRules.Entry],
    f: ['moveForward', TraversalRules.FormElement],
    F: ['moveBackward', TraversalRules.FormElement],
    g: ['moveForward', TraversalRules.Graphic],
    G: ['moveBackward', TraversalRules.Graphic],
    h: ['moveForward', TraversalRules.Heading],
    H: ['moveBackward', TraversalRules.Heading],
    i: ['moveForward', TraversalRules.ListItem],
    I: ['moveBackward', TraversalRules.ListItem],
    k: ['moveForward', TraversalRules.Link],
    K: ['moveBackward', TraversalRules.Link],
    l: ['moveForward', TraversalRules.List],
    L: ['moveBackward', TraversalRules.List],
    p: ['moveForward', TraversalRules.PageTab],
    P: ['moveBackward', TraversalRules.PageTab],
    r: ['moveForward', TraversalRules.RadioButton],
    R: ['moveBackward', TraversalRules.RadioButton],
    s: ['moveForward', TraversalRules.Separator],
    S: ['moveBackward', TraversalRules.Separator],
    t: ['moveForward', TraversalRules.Table],
    T: ['moveBackward', TraversalRules.Table],
    x: ['moveForward', TraversalRules.Checkbox],
    X: ['moveBackward', TraversalRules.Checkbox]
  }
};
