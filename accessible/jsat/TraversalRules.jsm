/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global PrefCache, Roles, Prefilters, States, Filters, Utils,
   TraversalRules, Components, XPCOMUtils */
/* exported TraversalRules */

'use strict';

const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ['TraversalRules']; // jshint ignore:line

Cu.import('resource://gre/modules/accessibility/Utils.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Roles',  // jshint ignore:line
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Filters',  // jshint ignore:line
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'States',  // jshint ignore:line
  'resource://gre/modules/accessibility/Constants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Prefilters',  // jshint ignore:line
  'resource://gre/modules/accessibility/Constants.jsm');

let gSkipEmptyImages = new PrefCache('accessibility.accessfu.skip_empty_images');

function BaseTraversalRule(aRoles, aMatchFunc, aPreFilter) {
  this._explicitMatchRoles = new Set(aRoles);
  this._matchRoles = aRoles;
  if (aRoles.indexOf(Roles.LABEL) < 0) {
    this._matchRoles.push(Roles.LABEL);
  }
  if (aRoles.indexOf(Roles.INTERNAL_FRAME) < 0) {
    // Used for traversing in to child OOP frames.
    this._matchRoles.push(Roles.INTERNAL_FRAME);
  }
  this._matchFunc = aMatchFunc || function() { return Filters.MATCH; };
  this.preFilter = aPreFilter || gSimplePreFilter;
}

BaseTraversalRule.prototype = {
    getMatchRoles: function BaseTraversalRule_getmatchRoles(aRules) {
      aRules.value = this._matchRoles;
      return aRules.value.length;
    },

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
   Roles.LISTITEM,
   Roles.GRID_CELL,
   Roles.COLUMNHEADER,
   Roles.ROWHEADER,
   Roles.STATUSBAR,
   Roles.SWITCH];

var gSimpleMatchFunc = function gSimpleMatchFunc(aAccessible) {
  // An object is simple, if it either has a single child lineage,
  // or has a flat subtree.
  function isSingleLineage(acc) {
    for (let child = acc; child; child = child.firstChild) {
      if (Utils.visibleChildCount(child) > 1) {
        return false;
      }
    }
    return true;
  }

  function isFlatSubtree(acc) {
    for (let child = acc.firstChild; child; child = child.nextSibling) {
      // text leafs inherit the actionCount of any ancestor that has a click
      // listener.
      if ([Roles.TEXT_LEAF, Roles.STATICTEXT].indexOf(child.role) >= 0) {
        continue;
      }
      if (Utils.visibleChildCount(child) > 0 || child.actionCount > 0) {
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
      return (name && name.trim()) ? Filters.MATCH : Filters.IGNORE;
    }
  case Roles.STATICTEXT:
    // Ignore prefix static text in list items. They are typically bullets or numbers.
    return Utils.isListItemDecorator(aAccessible) ?
      Filters.IGNORE : Filters.MATCH;
  case Roles.GRAPHIC:
    return TraversalRules._shouldSkipImage(aAccessible);
  case Roles.HEADER:
  case Roles.HEADING:
  case Roles.COLUMNHEADER:
  case Roles.ROWHEADER:
  case Roles.STATUSBAR:
    if ((aAccessible.childCount > 0 || aAccessible.name) &&
        (isSingleLineage(aAccessible) || isFlatSubtree(aAccessible))) {
      return Filters.MATCH | Filters.IGNORE_SUBTREE;
    }
    return Filters.IGNORE;
  case Roles.GRID_CELL:
    return isSingleLineage(aAccessible) || isFlatSubtree(aAccessible) ?
      Filters.MATCH | Filters.IGNORE_SUBTREE : Filters.IGNORE;
  case Roles.LISTITEM:
    {
      let item = aAccessible.childCount === 2 &&
        aAccessible.firstChild.role === Roles.STATICTEXT ?
        aAccessible.lastChild : aAccessible;
        return isSingleLineage(item) || isFlatSubtree(item) ?
          Filters.MATCH | Filters.IGNORE_SUBTREE : Filters.IGNORE;
    }
  default:
    // Ignore the subtree, if there is one. So that we don't land on
    // the same content that was already presented by its parent.
    return Filters.MATCH |
      Filters.IGNORE_SUBTREE;
  }
};

var gSimplePreFilter = Prefilters.DEFUNCT |
  Prefilters.INVISIBLE |
  Prefilters.ARIA_HIDDEN |
  Prefilters.TRANSPARENT;

this.TraversalRules = { // jshint ignore:line
  Simple: new BaseTraversalRule(gSimpleTraversalRoles, gSimpleMatchFunc),

  SimpleOnScreen: new BaseTraversalRule(
    gSimpleTraversalRoles, gSimpleMatchFunc,
    Prefilters.DEFUNCT | Prefilters.INVISIBLE | Prefilters.ARIA_HIDDEN |
    Prefilters.TRANSPARENT | Prefilters.OFFSCREEN),

  Anchor: new BaseTraversalRule(
    [Roles.LINK],
    function Anchor_match(aAccessible)
    {
      // We want to ignore links, only focus named anchors.
      if (Utils.getState(aAccessible).contains(States.LINKED)) {
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
     Roles.CHECK_MENU_ITEM,
     Roles.SWITCH]),

  Graphic: new BaseTraversalRule(
    [Roles.GRAPHIC],
    function Graphic_match(aAccessible) {
      return TraversalRules._shouldSkipImage(aAccessible);
    }),

  Heading: new BaseTraversalRule(
    [Roles.HEADING],
    function Heading_match(aAccessible) {
      return aAccessible.childCount > 0 ? Filters.MATCH : Filters.IGNORE;
    }),

  ListItem: new BaseTraversalRule(
    [Roles.LISTITEM,
     Roles.TERM]),

  Link: new BaseTraversalRule(
    [Roles.LINK],
    function Link_match(aAccessible)
    {
      // We want to ignore anchors, only focus real links.
      if (Utils.getState(aAccessible).contains(States.LINKED)) {
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
     Roles.CHECK_MENU_ITEM,
     Roles.SWITCH /* A type of checkbox that represents on/off values */]),

  _shouldSkipImage: function _shouldSkipImage(aAccessible) {
    if (gSkipEmptyImages.value && aAccessible.name === '') {
      return Filters.IGNORE;
    }
    return Filters.MATCH;
  }
};
