/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const INCLUDE_DESC = 0x01;
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
   * @return {Array} Two string array. The first string describes the object
   *    and its states. The second string is the object's name. Whether the
   *    object's description or it's role is included is determined by
   *    {@link verbosityRoleMap}.
   */
  genForObject: function genForObject(aAccessible) {
    let roleString = gAccRetrieval.getStringRole(aAccessible.role);

    let func = this.objectUtteranceFunctions[roleString] ||
      this.objectUtteranceFunctions.defaultFunc;

    let flags = this.verbosityRoleMap[roleString] || 0;

    if (aAccessible.childCount == 0)
      flags |= INCLUDE_NAME;

    let state = {};
    let extState = {};
    aAccessible.getState(state, extState);
    let states = {base: state.value, ext: extState.value};

    return func.apply(this, [aAccessible, roleString, states, flags]);
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
    'menubar': INCLUDE_DESC,
    'scrollbar': INCLUDE_DESC,
    'grip': INCLUDE_DESC,
    'alert': INCLUDE_DESC | INCLUDE_NAME,
    'menupopup': INCLUDE_DESC,
    'menuitem': INCLUDE_DESC,
    'tooltip': INCLUDE_DESC,
    'application': INCLUDE_NAME,
    'document': INCLUDE_NAME,
    'grouping': INCLUDE_DESC | INCLUDE_NAME,
    'toolbar': INCLUDE_DESC,
    'table': INCLUDE_DESC | INCLUDE_NAME,
    'link': INCLUDE_DESC,
    'list': INCLUDE_DESC,
    'listitem': INCLUDE_DESC,
    'outline': INCLUDE_DESC,
    'outlineitem': INCLUDE_DESC,
    'pagetab': INCLUDE_DESC,
    'graphic': INCLUDE_DESC,
    'pushbutton': INCLUDE_DESC,
    'checkbutton': INCLUDE_DESC,
    'radiobutton': INCLUDE_DESC,
    'combobox': INCLUDE_DESC,
    'droplist': INCLUDE_DESC,
    'progressbar': INCLUDE_DESC,
    'slider': INCLUDE_DESC,
    'spinbutton': INCLUDE_DESC,
    'diagram': INCLUDE_DESC,
    'animation': INCLUDE_DESC,
    'equation': INCLUDE_DESC,
    'buttonmenu': INCLUDE_DESC,
    'pagetablist': INCLUDE_DESC,
    'canvas': INCLUDE_DESC,
    'check menu item': INCLUDE_DESC,
    'label': INCLUDE_DESC,
    'password text': INCLUDE_DESC,
    'popup menu': INCLUDE_DESC,
    'radio menu item': INCLUDE_DESC,
    'toggle button': INCLUDE_DESC,
    'header': INCLUDE_DESC,
    'footer': INCLUDE_DESC,
    'entry': INCLUDE_DESC | INCLUDE_NAME,
    'caption': INCLUDE_DESC,
    'document frame': INCLUDE_DESC,
    'heading': INCLUDE_DESC,
    'calendar': INCLUDE_DESC | INCLUDE_NAME,
    'combobox list': INCLUDE_DESC,
    'combobox option': INCLUDE_DESC,
    'image map': INCLUDE_DESC,
    'option': INCLUDE_DESC,
    'listbox': INCLUDE_DESC},

  objectUtteranceFunctions: {
    defaultFunc: function defaultFunc(aAccessible, aRoleStr, aStates, aFlags) {
      let utterance = [];

      if (aFlags & INCLUDE_DESC) {
        let desc = this._getLocalizedStates(aStates);
        let roleStr = this._getLocalizedRole(aRoleStr);
        if (roleStr)
          desc.push(roleStr);
        utterance.push(desc.join(' '));
      }

      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      if (name)
        utterance.push(name);

      return utterance;
    },

    entry: function entry(aAccessible, aRoleStr, aStates, aFlags) {
      let utterance = [];
      let desc = this._getLocalizedStates(aStates);
      desc.push(this._getLocalizedRole(
                  (aStates.ext & Ci.nsIAccessibleStates.EXT_STATE_MULTI_LINE) ?
                    'textarea' : 'entry'));

      utterance.push(desc.join(' '));

      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      if (name)
        utterance.push(name);

      return utterance;
    },

    heading: function heading(aAccessible, aRoleStr, aStates, aFlags) {
      let name = (aFlags & INCLUDE_NAME) ? (aAccessible.name || '') : '';
      let level = {};
      aAccessible.groupPosition(level, {}, {});
      let utterance =
        [gStringBundle.formatStringFromName('headingLevel', [level.value], 1)];

      if (name)
        utterance.push(name);

      return utterance;
    },

    listitem: function listitem(aAccessible, aRoleStr, aStates, aFlags) {
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
  },

  _getLocalizedStates: function _getLocalizedStates(aStates) {
    let stateUtterances = [];

    if (aStates.base & Ci.nsIAccessibleStates.STATE_UNAVAILABLE) {
      stateUtterances.push(gStringBundle.GetStringFromName('stateUnavailable'));
    }

    if (aStates.base & Ci.nsIAccessibleStates.STATE_CHECKABLE) {
      let stateStr = (aStates.base & Ci.nsIAccessibleStates.STATE_CHECKED) ?
        'stateChecked' : 'stateNotChecked';
      stateUtterances.push(gStringBundle.GetStringFromName(stateStr));
    }

    if (aStates.ext & Ci.nsIAccessibleStates.EXT_STATE_EXPANDABLE) {
      let stateStr = (aStates.base & Ci.nsIAccessibleStates.STATE_EXPANDED) ?
        'stateExpanded' : 'stateCollapsed';
      stateUtterances.push(gStringBundle.GetStringFromName(stateStr));
    }

    if (aStates.base & Ci.nsIAccessibleStates.STATE_REQUIRED) {
      stateUtterances.push(gStringBundle.GetStringFromName('stateRequired'));
    }

    if (aStates.base & Ci.nsIAccessibleStates.STATE_TRAVERSED) {
      stateUtterances.push(gStringBundle.GetStringFromName('stateTraversed'));
    }

    return stateUtterances;
  }
};
