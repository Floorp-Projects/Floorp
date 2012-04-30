/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const INCLUDE_ROLE = 0x01;
const INCLUDE_NAME = 0x02;
const INCLUDE_CUSTOM = 0x04;

var gStringBundle = Cc['@mozilla.org/intl/stringbundle;1'].
  getService(Ci.nsIStringBundleService).
  createBundle('chrome://global/locale/AccessFu.properties');

var gAccRetrieval = Cc['@mozilla.org/accessibleRetrieval;1'].
  getService(Ci.nsIAccessibleRetrieval);

var EXPORTED_SYMBOLS = ['UtteranceGenerator'];

var UtteranceGenerator = {
  gActionMap: {
    jump: 'jumpAction',
    press: 'pressAction',
    check: 'checkAction',
    uncheck: 'uncheckAction',
    select: 'selectAction',
    open: 'openAction',
    close: 'closeAction',
    switch: 'switchAction',
    click: 'clickAction',
    collapse: 'collapseAction',
    expand: 'expandAction',
    activate: 'activateAction',
    cycle: 'cycleAction'
  },

  genForObject: function(aAccessible, aForceName) {
    let roleString = gAccRetrieval.getStringRole(aAccessible.role);

    let func = this.objectUtteranceFunctions[roleString] ||
      this.objectUtteranceFunctions.defaultFunc;

    let flags = this.verbosityRoleMap[roleString] || 0;

    if (aForceName)
      flags |= INCLUDE_NAME;

    return func(aAccessible, roleString, flags);
  },

  genForAction: function(aObject, aActionName) {
    return [gStringBundle.GetStringFromName(this.gActionMap[aActionName])];
  },

  verbosityRoleMap: {
    'menubar': INCLUDE_ROLE,
    'scrollbar': INCLUDE_ROLE,
    'grip': INCLUDE_ROLE,
    'alert': INCLUDE_ROLE,
    'menupopup': INCLUDE_ROLE,
    'menuitem': INCLUDE_ROLE,
    'tooltip': INCLUDE_ROLE,
    'application': INCLUDE_NAME,
    'document': INCLUDE_NAME,
    'toolbar': INCLUDE_ROLE,
    'link': INCLUDE_ROLE,
    'list': INCLUDE_ROLE,
    'listitem': INCLUDE_ROLE,
    'outline': INCLUDE_ROLE,
    'outlineitem': INCLUDE_ROLE,
    'pagetab': INCLUDE_ROLE,
    'graphic': INCLUDE_ROLE | INCLUDE_NAME,
    'statictext': INCLUDE_NAME,
    'text leaf': INCLUDE_NAME,
    'pushbutton': INCLUDE_ROLE,
    'checkbutton': INCLUDE_ROLE | INCLUDE_NAME,
    'radiobutton': INCLUDE_ROLE | INCLUDE_NAME,
    'combobox': INCLUDE_ROLE,
    'droplist': INCLUDE_ROLE,
    'progressbar': INCLUDE_ROLE,
    'slider': INCLUDE_ROLE,
    'spinbutton': INCLUDE_ROLE,
    'diagram': INCLUDE_ROLE,
    'animation': INCLUDE_ROLE,
    'equation': INCLUDE_ROLE,
    'buttonmenu': INCLUDE_ROLE,
    'pagetablist': INCLUDE_ROLE,
    'canvas': INCLUDE_ROLE,
    'check menu item': INCLUDE_ROLE,
    'label': INCLUDE_ROLE,
    'password text': INCLUDE_ROLE,
    'popup menu': INCLUDE_ROLE,
    'radio menu item': INCLUDE_ROLE,
    'toggle button': INCLUDE_ROLE,
    'header': INCLUDE_ROLE,
    'footer': INCLUDE_ROLE,
    'entry': INCLUDE_ROLE,
    'caption': INCLUDE_ROLE,
    'document frame': INCLUDE_ROLE,
    'heading': INCLUDE_ROLE,
    'calendar': INCLUDE_ROLE,
    'combobox list': INCLUDE_ROLE,
    'combobox option': INCLUDE_ROLE,
    'image map': INCLUDE_ROLE,
    'option': INCLUDE_ROLE,
    'listbox': INCLUDE_ROLE},

  objectUtteranceFunctions: {
    defaultFunc: function defaultFunc(aAccessible, aRoleStr, aFlags) {
      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      let desc = (aFlags & INCLUDE_ROLE) ?
        gStringBundle.GetStringFromName(aRoleStr) : '';

      if (!name && !desc)
        return [];

      let state = {};
      let extState = {};
      aAccessible.getState(state, extState);

      if (state.value & Ci.nsIAccessibleStates.STATE_CHECKABLE) {
        let stateStr = (state.value & Ci.nsIAccessibleStates.STATE_CHECKED) ?
          'objChecked' : 'objNotChecked';
        desc = gStringBundle.formatStringFromName(stateStr, [desc], 1);
      }

      if (extState.value & Ci.nsIAccessibleStates.EXT_STATE_EXPANDABLE) {
        let stateStr = (state.value & Ci.nsIAccessibleStates.STATE_EXPANDED) ?
          'objExpanded' : 'objCollapsed';
        desc = gStringBundle.formatStringFromName(stateStr, [desc], 1);
      }

      return [desc, name];
    },

    heading: function(aAccessible, aRoleStr, aFlags) {
      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      let level = {};
      aAccessible.groupPosition(level, {}, {});
      let desc = gStringBundle.formatStringFromName('headingLevel',
                                                   [level.value], 1);
      return [desc, name];
    },

    listitem: function(aAccessible, aRoleStr, aFlags) {
      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      let localizedRole = gStringBundle.GetStringFromName(aRoleStr);
      let itemno = {};
      let itemof = {};
      aAccessible.groupPosition({}, itemof, itemno);
      let desc = gStringBundle.formatStringFromName(
          'objItemOf', [localizedRole, itemno.value, itemof.value], 3);

      return [desc, name];
    }
  }
};
