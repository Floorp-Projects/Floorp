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

var TraversalRules = {
  Simple: {
    getMatchRoles: function SimpleTraversalRule_getmatchRoles(aRules) {
      aRules.value = this._matchRoles;
      return aRules.value.length;
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
  },

  Anchor: {
    getMatchRoles: function AnchorTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_LINK];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function AnchorTraversalRule_match(aAccessible)
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
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  Button: {
    getMatchRoles: function ButtonTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = this._matchRoles;
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function ButtonTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule]),

    _matchRoles: [
      Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
      Ci.nsIAccessibleRole.ROLE_SPINBUTTON,
      Ci.nsIAccessibleRole.ROLE_TOGGLE_BUTTON,
      Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWN,
      Ci.nsIAccessibleRole.ROLE_BUTTONDROPDOWNGRID
    ]
  },

  Combobox: {
    getMatchRoles: function ComboboxTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_COMBOBOX,
                      Ci.nsIAccessibleRole.ROLE_LISTBOX];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function ComboboxTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  Entry: {
    getMatchRoles: function EntryTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_ENTRY,
                      Ci.nsIAccessibleRole.ROLE_PASSWORD_TEXT];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function EntryTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  FormElement: {
    getMatchRoles: function FormElementTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = this._matchRoles;
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function FormElementTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule]),

    _matchRoles: [
      Ci.nsIAccessibleRole.ROLE_PUSHBUTTON,
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
      Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM
    ]
  },

  Graphic: {
    getMatchRoles: function GraphicTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_GRAPHIC];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function GraphicTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  Heading: {
    getMatchRoles: function HeadingTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_HEADING];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function HeadingTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  ListItem: {
    getMatchRoles: function ListItemTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_LISTITEM,
                      Ci.nsIAccessibleRole.ROLE_TERM];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function ListItemTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  Link: {
    getMatchRoles: function LinkTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_LINK];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function LinkTraversalRule_match(aAccessible)
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
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  List: {
    getMatchRoles: function ListTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_LIST,
                      Ci.nsIAccessibleRole.ROLE_DEFINITION_LIST];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function ListTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  PageTab: {
    getMatchRoles: function PageTabTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_PAGETAB];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function PageTabTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  RadioButton: {
    getMatchRoles: function RadioButtonTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_RADIOBUTTON,
                      Ci.nsIAccessibleRole.ROLE_RADIO_MENU_ITEM];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function RadioButtonTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  Separator: {
    getMatchRoles: function SeparatorTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_SEPARATOR];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function SeparatorTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  Table: {
    getMatchRoles: function TableTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_TABLE];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function TableTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  },

  Checkbox: {
    getMatchRoles: function CheckboxTraversalRule_getMatchRoles(aRules)
    {
      aRules.value = [Ci.nsIAccessibleRole.ROLE_CHECKBUTTON,
                      Ci.nsIAccessibleRole.ROLE_CHECK_MENU_ITEM];
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
      Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE,

    match: function CheckboxTraversalRule_match(aAccessible)
    {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
  }
};

var VirtualCursorController = {
  NOT_EDITABLE: 0,
  SINGLE_LINE_EDITABLE: 1,
  MULTI_LINE_EDITABLE: 2,

  exploreByTouch: false,

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
        if (this._isEditableText(target) ||
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
        this.moveForward(document, true);
        break;
      case aEvent.DOM_VK_HOME:
        this.moveBackward(document, true);
        break;
      case aEvent.DOM_VK_RIGHT:
        if (this._isEditableText(target)) {
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
        if (this._isEditableText(target)) {
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
        if (this._isEditableText(target) == this.MULTI_LINE_EDITABLE) {
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

  moveToPoint: function moveToPoint(aDocument, aX, aY) {
    this.getVirtualCursor(aDocument).moveToPoint(TraversalRules.Simple,
                                                 aX, aY, true);
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

  moveForward: function moveForward(aDocument, aLast, aRule) {
    let virtualCursor = this.getVirtualCursor(aDocument);
    if (aLast) {
      virtualCursor.moveLast(TraversalRules.Simple);
    } else {
      try {
        virtualCursor.moveNext(aRule || TraversalRules.Simple);
      } catch (x) {
        this.moveCursorToObject(
            gAccRetrieval.getAccessibleFor(aDocument.activeElement), aRule);
      }
    }
  },

  moveBackward: function moveBackward(aDocument, aFirst, aRule) {
    let virtualCursor = this.getVirtualCursor(aDocument);
    if (aFirst) {
      virtualCursor.moveFirst(TraversalRules.Simple);
    } else {
      try {
        virtualCursor.movePrevious(aRule || TraversalRules.Simple);
      } catch (x) {
        this.moveCursorToObject(
            gAccRetrieval.getAccessibleFor(aDocument.activeElement), aRule);
      }
    }
  },

  activateCurrent: function activateCurrent(document) {
    let virtualCursor = this.getVirtualCursor(document);
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

  getVirtualCursor: function getVirtualCursor(document) {
    return gAccRetrieval.getAccessibleFor(document).
      QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
  },

  moveCursorToObject: function moveCursorToObject(aAccessible, aRule) {
    let doc = aAccessible.document;
    while (doc) {
      let vc = null;
      try {
        vc = doc.QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
      } catch (x) {
        doc = doc.parentDocument;
        continue;
      }
      vc.moveNext(aRule || TraversalRules.Simple, aAccessible, true);
      break;
    }
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
