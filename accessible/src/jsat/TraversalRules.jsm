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
XPCOMUtils.defineLazyModuleGetter(this, 'Roles',
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Filters',
  'resource://gre/modules/accessibility/Constants.jsm');

let gSkipEmptyImages = new PrefCache('accessibility.accessfu.skip_empty_images');

function BaseTraversalRule(aRoles, aMatchFunc) {
  this._explicitMatchRoles = new Set(aRoles);
  this._matchRoles = aRoles;
  if (aRoles.indexOf(Roles.LABEL) < 0) {
    this._matchRoles.push(Roles.LABEL);
  }
  this._matchFunc = aMatchFunc || function (acc) { return Filters.MATCH; };
}

BaseTraversalRule.prototype = {
    getMatchRoles: function BaseTraversalRule_getmatchRoles(aRules) {
      aRules.value = this._matchRoles;
      return aRules.value.length;
    },

    preFilter: Ci.nsIAccessibleTraversalRule.PREFILTER_DEFUNCT |
    Ci.nsIAccessibleTraversalRule.PREFILTER_INVISIBLE |
    Ci.nsIAccessibleTraversalRule.PREFILTER_ARIA_HIDDEN |
    Ci.nsIAccessibleTraversalRule.PREFILTER_TRANSPARENT,

    match: function BaseTraversalRule_match(aAccessible)
    {
      let role = aAccessible.role;
      if (role == Roles.INTERNAL_FRAME) {
        return (Utils.getMessageManager(aAccessible.DOMNode)) ?
          Filters.MATCH  | Filters.IGNORE_SUBTREE : Filters.IGNORE;
      }

      let matchResult = this._explicitMatchRoles.has(role) ?
          this._matchFunc(aAccessible) : Filters.IGNORE;

      // If we are on a label that nests a checkbox/radio we should land on it.
      // It is a bigger touch target, and it reduces clutter.
      if (role == Roles.LABEL && !(matchResult & Filters.IGNORE_SUBTREE)) {
        let control = Utils.getEmbeddedControl(aAccessible);
        if (control && this._explicitMatchRoles.has(control.role)) {
          matchResult = this._matchFunc(control) | Filters.IGNORE_SUBTREE;
        }
      }

      return matchResult;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAccessibleTraversalRule])
};

var gSimpleTraversalRoles =
  [Roles.MENUITEM,
   Roles.LINK,
   Roles.PAGETAB,
   Roles.GRAPHIC,
   Roles.STATICTEXT,
   Roles.TEXT_LEAF,
   Roles.PUSHBUTTON,
   Roles.CHECKBUTTON,
   Roles.RADIOBUTTON,
   Roles.COMBOBOX,
   Roles.PROGRESSBAR,
   Roles.BUTTONDROPDOWN,
   Roles.BUTTONMENU,
   Roles.CHECK_MENU_ITEM,
   Roles.PASSWORD_TEXT,
   Roles.RADIO_MENU_ITEM,
   Roles.TOGGLE_BUTTON,
   Roles.ENTRY,
   Roles.KEY,
   Roles.HEADER,
   Roles.HEADING,
   Roles.SLIDER,
   Roles.SPINBUTTON,
   Roles.OPTION,
   // Used for traversing in to child OOP frames.
   Roles.INTERNAL_FRAME];

this.TraversalRules = {
  Simple: new BaseTraversalRule(
    gSimpleTraversalRoles,
    function Simple_match(aAccessible) {
      function hasZeroOrSingleChildDescendants () {
        for (let acc = aAccessible; acc.childCount > 0; acc = acc.firstChild) {
          if (acc.childCount > 1) {
            return false;
          }
        }

        return true;
      }

      switch (aAccessible.role) {
      case Roles.COMBOBOX:
        // We don't want to ignore the subtree because this is often
        // where the list box hangs out.
        return Filters.MATCH;
      case Roles.TEXT_LEAF:
        {
          // Nameless text leaves are boring, skip them.
          let name = aAccessible.name;
          if (name && name.trim())
            return Filters.MATCH;
          else
            return Filters.IGNORE;
        }
      case Roles.STATICTEXT:
        {
          let parent = aAccessible.parent;
          // Ignore prefix static text in list items. They are typically bullets or numbers.
          if (parent.childCount > 1 && aAccessible.indexInParent == 0 &&
              parent.role == Roles.LISTITEM)
            return Filters.IGNORE;

          return Filters.MATCH;
        }
      case Roles.GRAPHIC:
        return TraversalRules._shouldSkipImage(aAccessible);
      case Roles.LINK:
      case Roles.HEADER:
      case Roles.HEADING:
        return hasZeroOrSingleChildDescendants() ?
          (Filters.MATCH | Filters.IGNORE_SUBTREE) : (Filters.IGNORE);
      default:
        // Ignore the subtree, if there is one. So that we don't land on
        // the same content that was already presented by its parent.
        return Filters.MATCH |
          Filters.IGNORE_SUBTREE;
      }
    }
  ),

  Anchor: new BaseTraversalRule(
    [Roles.LINK],
    function Anchor_match(aAccessible)
    {
      // We want to ignore links, only focus named anchors.
      let state = {};
      let extraState = {};
      aAccessible.getState(state, extraState);
      if (state.value & Ci.nsIAccessibleStates.STATE_LINKED) {
        return Filters.IGNORE;
      } else {
        return Filters.MATCH;
      }
    }),

  Button: new BaseTraversalRule(
    [Roles.PUSHBUTTON,
     Roles.SPINBUTTON,
     Roles.TOGGLE_BUTTON,
     Roles.BUTTONDROPDOWN,
     Roles.BUTTONDROPDOWNGRID]),

  Combobox: new BaseTraversalRule(
    [Roles.COMBOBOX,
     Roles.LISTBOX]),

  Landmark: new BaseTraversalRule(
    [],
    function Landmark_match(aAccessible) {
      return Utils.getLandmarkName(aAccessible) ? Filters.MATCH :
        Filters.IGNORE;
    }
  ),

  Entry: new BaseTraversalRule(
    [Roles.ENTRY,
     Roles.PASSWORD_TEXT]),

  FormElement: new BaseTraversalRule(
    [Roles.PUSHBUTTON,
     Roles.SPINBUTTON,
     Roles.TOGGLE_BUTTON,
     Roles.BUTTONDROPDOWN,
     Roles.BUTTONDROPDOWNGRID,
     Roles.COMBOBOX,
     Roles.LISTBOX,
     Roles.ENTRY,
     Roles.PASSWORD_TEXT,
     Roles.PAGETAB,
     Roles.RADIOBUTTON,
     Roles.RADIO_MENU_ITEM,
     Roles.SLIDER,
     Roles.CHECKBUTTON,
     Roles.CHECK_MENU_ITEM]),

  Graphic: new BaseTraversalRule(
    [Roles.GRAPHIC],
    function Graphic_match(aAccessible) {
      return TraversalRules._shouldSkipImage(aAccessible);
    }),

  Heading: new BaseTraversalRule(
    [Roles.HEADING]),

  ListItem: new BaseTraversalRule(
    [Roles.LISTITEM,
     Roles.TERM]),

  Link: new BaseTraversalRule(
    [Roles.LINK],
    function Link_match(aAccessible)
    {
      // We want to ignore anchors, only focus real links.
      let state = {};
      let extraState = {};
      aAccessible.getState(state, extraState);
      if (state.value & Ci.nsIAccessibleStates.STATE_LINKED) {
        return Filters.MATCH;
      } else {
        return Filters.IGNORE;
      }
    }),

  List: new BaseTraversalRule(
    [Roles.LIST,
     Roles.DEFINITION_LIST]),

  PageTab: new BaseTraversalRule(
    [Roles.PAGETAB]),

  Paragraph: new BaseTraversalRule(
    [Roles.PARAGRAPH,
     Roles.SECTION],
    function Paragraph_match(aAccessible) {
      for (let child = aAccessible.firstChild; child; child = child.nextSibling) {
        if (child.role === Roles.TEXT_LEAF) {
          return Filters.MATCH | Filters.IGNORE_SUBTREE;
        }
      }

      return Filters.IGNORE;
    }),

  RadioButton: new BaseTraversalRule(
    [Roles.RADIOBUTTON,
     Roles.RADIO_MENU_ITEM]),

  Separator: new BaseTraversalRule(
    [Roles.SEPARATOR]),

  Table: new BaseTraversalRule(
    [Roles.TABLE]),

  Checkbox: new BaseTraversalRule(
    [Roles.CHECKBUTTON,
     Roles.CHECK_MENU_ITEM]),

  _shouldSkipImage: function _shouldSkipImage(aAccessible) {
    if (gSkipEmptyImages.value && aAccessible.name === '') {
      return Filters.IGNORE;
    }
    return Filters.MATCH;
  }
};
