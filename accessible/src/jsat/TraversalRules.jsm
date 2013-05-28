/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

this.EXPORTED_SYMBOLS = ['TraversalRules'];

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

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
    Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE |
    Ci.nsIAccessibleTraversalRule.PREFILTER_ARIA_HIDDEN,

    match: function BaseTraversalRule_match(aAccessible)
    {
      if (aAccessible.role == Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME) {
        return (Utils.getMessageManager(aAccessible.DOMNode)) ?
          Ci.nsIAccessibleTraversalRule.FILTER_MATCH :
          Ci.nsIAccessibleTraversalRule.FILTER_IGNORE;
      }

      if (this._matchFunc)
        return this._matchFunc(aAccessible);

      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
};

var gSimpleTraversalRoles =
  [Ci.nsIAccessibleRole.ROLE_MENUITEM,
   Ci.nsIAccessibleRole.ROLE_LINK,
   Ci.nsIAccessibleRole.ROLE_PAGETAB,
   Ci.nsIAccessibleRole.ROLE_GRAPHIC,
   Ci.nsIAccessibleRole.ROLE_STATICTEXT,
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
   Ci.nsIAccessibleRole.ROLE_ENTRY,
   // Used for traversing in to child OOP frames.
   Ci.nsIAccessibleRole.ROLE_INTERNAL_FRAME];

this.TraversalRules = {
  Simple: new BaseTraversalRule(
    gSimpleTraversalRoles,
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
      case Ci.nsIAccessibleRole.ROLE_STATICTEXT:
        {
          let parent = aAccessible.parent;
          // Ignore prefix static text in list items. They are typically bullets or numbers.
          if (parent.childCount > 1 && aAccessible.indexInParent == 0 &&
              parent.role == Ci.nsIAccessibleRole.ROLE_LISTITEM)
            return Ci.nsIAccessibleTraversalRule.FILTER_IGNORE;

          return Ci.nsIAccessibleTraversalRule.FILTER_MATCH;
        }
      default:
        // Ignore the subtree, if there is one. So that we don't land on
        // the same content that was already presented by its parent.
        return Ci.nsIAccessibleTraversalRule.FILTER_MATCH |
          Ci.nsIAccessibleTraversalRule.FILTER_IGNORE_SUBTREE;
      }
    }
  ),

  SimpleTouch: new BaseTraversalRule(
    gSimpleTraversalRoles,
    function Simple_match(aAccessible) {
      return Ci.nsIAccessibleTraversalRule.FILTER_MATCH |
        Ci.nsIAccessibleTraversalRule.FILTER_IGNORE_SUBTREE;
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
