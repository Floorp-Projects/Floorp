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

/**
 * Generates speech utterances from objects, actions and state changes.
 * An utterance is an array of strings.
 *
 * It should not be assumed that flattening an utterance array would create a
 * gramatically correct sentence. For example, {@link genForObject} might
 * return: ['graphic', 'Welcome to my home page'].
 * Each string element in an utterance should be gramatically correct in itself.
 * Another example from {@link genForObject}: ['list item 2 of 5', 'Alabama'].
 *
 * An utterance is ordered from the least to the most important. Speaking the
 * last string usually makes sense, but speaking the first often won't.
 * For example {@link genForAction} might return ['button', 'clicked'] for a
 * clicked event. Speaking only 'clicked' makes sense. Speaking 'button' does
 * not.
 */
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


  /**
   * Generates an utterance for an object.
   * @param {nsIAccessible} aAccessible accessible object to generate utterance
   *    for.
   * @param {boolean} aForceName include the object's name in the utterance
   *    even if this object type does not usually have it's name uttered.
   * @return {Array} Two string array. The first string describes the object
   *    and its states. The second string is the object's name. Some object
   *    types may have the description or name omitted, instead an empty string
   *    is returned as a placeholder. Whether the object's description or it's
   *    role is included is determined by {@link verbosityRoleMap}.
   */
  genForObject: function genForObject(aAccessible, aForceName) {
    let roleString = gAccRetrieval.getStringRole(aAccessible.role);

    let func = this.objectUtteranceFunctions[roleString] ||
      this.objectUtteranceFunctions.defaultFunc;

    let flags = this.verbosityRoleMap[roleString] || 0;

    if (aForceName)
      flags |= INCLUDE_NAME;

    return func.apply(this, [aAccessible, roleString, flags]);
  },

  /**
   * Generates an utterance for an action performed.
   * TODO: May become more verbose in the future.
   * @param {nsIAccessible} aAccessible accessible object that the action was
   *    invoked in.
   * @param {string} aActionName the name of the action, one of the keys in
   *    {@link gActionMap}.
   * @return {Array} A one string array with the action.
   */
  genForAction: function genForAction(aObject, aActionName) {
    return [gStringBundle.GetStringFromName(this.gActionMap[aActionName])];
  },

  /**
   * Generates an utterance for a tab state change.
   * @param {nsIAccessible} aAccessible accessible object of the tab's attached
   *    document.
   * @param {string} aTabState the tab state name, see
   *    {@link Presenter.tabStateChanged}.
   * @return {Array} The tab state utterace.
   */
  genForTabStateChange: function genForTabStateChange(aObject, aTabState) {
    switch (aTabState) {
      case 'newtab':
        return [gStringBundle.GetStringFromName('tabNew')];
      case 'loading':
        return [gStringBundle.GetStringFromName('tabLoading')];
      case 'loaded':
        return [aObject.name || '',
                gStringBundle.GetStringFromName('tabLoaded')];
      case 'loadstopped':
        return [gStringBundle.GetStringFromName('tabLoadStopped')];
      case 'reload':
        return [gStringBundle.GetStringFromName('tabReload')];
      default:
        return [];
    }
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
        this._getLocalizedRole(aRoleStr) : '';

      let utterance = [];

      if (desc) {
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

        utterance.push(desc);
      }

      if (name)
        utterance.push(name);

      return utterance;
    },

    heading: function heading(aAccessible, aRoleStr, aFlags) {
      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      let level = {};
      aAccessible.groupPosition(level, {}, {});
      let utterance =
        [gStringBundle.formatStringFromName('headingLevel', [level.value], 1)];

      if (name)
        utterance.push(name);

      return utterance;
    },

    listitem: function listitem(aAccessible, aRoleStr, aFlags) {
      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      let localizedRole = this._getLocalizedRole(aRoleStr);
      let itemno = {};
      let itemof = {};
      aAccessible.groupPosition({}, itemof, itemno);
      let utterance =
        [gStringBundle.formatStringFromName(
           'objItemOf', [localizedRole, itemno.value, itemof.value], 3)];

      if (name)
        utterance.push(name);

      return utterance;
    }
  },

  _getLocalizedRole: function _getLocalizedRole(aRoleStr) {
    try {
      return gStringBundle.GetStringFromName(aRoleStr.replace(' ', ''));
    } catch (x) {
      return '';
    }
  }
};
