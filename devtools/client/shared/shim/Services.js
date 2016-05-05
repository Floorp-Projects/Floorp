/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals localStorage, window */

// Some constants from nsIPrefBranch.idl.
const PREF_INVALID = 0;
const PREF_STRING = 32;
const PREF_INT = 64;
const PREF_BOOL = 128;
const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";

/**
 * Create a new preference object.
 *
 * @param {PrefBranch} branch the branch holding this preference
 * @param {String} name the base name of this preference
 * @param {String} fullName the fully-qualified name of this preference
 */
function Preference(branch, name, fullName) {
  this.branch = branch;
  this.name = name;
  this.fullName = fullName;
  this.defaultValue = null;
  this.hasUserValue = false;
  this.userValue = null;
  this.type = null;
}

Preference.prototype = {
  /**
   * Return this preference's current value.
   *
   * @return {Any} The current value of this preference.  This may
   *         return a string, a number, or a boolean depending on the
   *         preference's type.
   */
  get: function () {
    if (this.hasUserValue) {
      return this.userValue;
    }
    return this.defaultValue;
  },

  /**
   * Set the preference's value.  The new value is assumed to be a
   * user value.  After setting the value, this function emits a
   * change notification.
   *
   * @param {Any} value the new value
   */
  set: function (value) {
    if (!this.hasUserValue || value !== this.userValue) {
      this.userValue = value;
      this.hasUserValue = true;
      this.saveAndNotify();
    }
  },

  /**
   * Set the default value for this preference, and emit a
   * notification if this results in a visible change.
   *
   * @param {Any} value the new default value
   */
  setDefault: function (value) {
    if (this.defaultValue !== value) {
      this.defaultValue = value;
      if (!this.hasUserValue) {
        this.saveAndNotify();
      }
    }
  },

  /**
   * If this preference has a user value, clear it.  If a change was
   * made, emit a change notification.
   */
  clearUserValue: function () {
    if (this.hasUserValue) {
      this.userValue = null;
      this.hasUserValue = false;
      this.saveAndNotify();
    }
  },

  /**
   * Helper function to write the preference's value to local storage
   * and then emit a change notification.
   */
  saveAndNotify: function () {
    let store = {
      type: this.type,
      defaultValue: this.defaultValue,
      hasUserValue: this.hasUserValue,
      userValue: this.userValue,
    };

    localStorage.setItem(this.fullName, JSON.stringify(store));
    this.branch._notify(this.name);
  },

  /**
   * Change this preference's value without writing it back to local
   * storage.  This is used to handle changes to local storage that
   * were made externally.
   *
   * @param {Number} type one of the PREF_* values
   * @param {Any} userValue the user value to use if the pref does not exist
   * @param {Any} defaultValue the default value to use if the pref
   *        does not exist
   * @param {Boolean} hasUserValue if a new pref is created, whether
   *        the default value is also a user value
   * @param {Object} store the new value of the preference.  It should
   *        be of the form {type, defaultValue, hasUserValue, userValue};
   *        where |type| is one of the PREF_* type constants; |defaultValue|
   *        and |userValue| are the default and user values, respectively;
   *        and |hasUserValue| is a boolean indicating whether the user value
   *        is valid
   */
  storageUpdated: function (type, userValue, hasUserValue, defaultValue) {
    this.type = type;
    this.defaultValue = defaultValue;
    this.hasUserValue = hasUserValue;
    this.userValue = userValue;
    // There's no need to write this back to local storage, since it
    // came from there; and this avoids infinite event loops.
    this.branch._notify(this.name);
  },
};

/**
 * Create a new preference branch.  This object conforms largely to
 * nsIPrefBranch and nsIPrefService, though it only implements the
 * subset needed by devtools.
 *
 * @param {PrefBranch} parent the parent branch, or null for the root
 *        branch.
 * @param {String} name the base name of this branch
 * @param {String} fullName the fully-qualified name of this branch
 */
function PrefBranch(parent, name, fullName) {
  this._parent = parent;
  this._name = name;
  this._fullName = fullName;
  this._observers = {};
  this._children = {};

  if (!parent) {
    this._initializeRoot();
  }
}

PrefBranch.prototype = {
  PREF_INVALID: PREF_INVALID,
  PREF_STRING: PREF_STRING,
  PREF_INT: PREF_INT,
  PREF_BOOL: PREF_BOOL,

  /** @see nsIPrefBranch.root.  */
  get root() {
    return this._fullName;
  },

  /** @see nsIPrefBranch.getPrefType.  */
  getPrefType: function (prefName) {
    return this._findPref(prefName).type;
  },

  /** @see nsIPrefBranch.getBoolPref.  */
  getBoolPref: function (prefName) {
    let thePref = this._findPref(prefName);
    if (thePref.type !== PREF_BOOL) {
      throw new Error(`${prefName} does not have bool type`);
    }
    return thePref.get();
  },

  /** @see nsIPrefBranch.setBoolPref.  */
  setBoolPref: function (prefName, value) {
    if (typeof value !== "boolean") {
      throw new Error("non-bool passed to setBoolPref");
    }
    let thePref = this._findOrCreatePref(prefName, value, true, value);
    if (thePref.type !== PREF_BOOL) {
      throw new Error(`${prefName} does not have bool type`);
    }
    thePref.set(value);
  },

  /** @see nsIPrefBranch.getCharPref.  */
  getCharPref: function (prefName) {
    let thePref = this._findPref(prefName);
    if (thePref.type !== PREF_STRING) {
      throw new Error(`${prefName} does not have string type`);
    }
    return thePref.get();
  },

  /** @see nsIPrefBranch.setCharPref.  */
  setCharPref: function (prefName, value) {
    if (typeof value !== "string") {
      throw new Error("non-string passed to setCharPref");
    }
    let thePref = this._findOrCreatePref(prefName, value, true, value);
    if (thePref.type !== PREF_STRING) {
      throw new Error(`${prefName} does not have string type`);
    }
    thePref.set(value);
  },

  /** @see nsIPrefBranch.getIntPref.  */
  getIntPref: function (prefName) {
    let thePref = this._findPref(prefName);
    if (thePref.type !== PREF_INT) {
      throw new Error(`${prefName} does not have int type`);
    }
    return thePref.get();
  },

  /** @see nsIPrefBranch.setIntPref.  */
  setIntPref: function (prefName, value) {
    if (typeof value !== "number") {
      throw new Error("non-number passed to setIntPref");
    }
    let thePref = this._findOrCreatePref(prefName, value, true, value);
    if (thePref.type !== PREF_INT) {
      throw new Error(`${prefName} does not have int type`);
    }
    thePref.set(value);
  },

  /** @see nsIPrefBranch.clearUserPref */
  clearUserPref: function (prefName) {
    let thePref = this._findPref(prefName);
    thePref.clearUserValue();
  },

  /** @see nsIPrefBranch.prefHasUserValue */
  prefHasUserValue: function (prefName) {
    let thePref = this._findPref(prefName);
    return thePref.hasUserValue;
  },

  /** @see nsIPrefBranch.addObserver */
  addObserver: function (domain, observer, holdWeak) {
    if (domain !== "" && !domain.endsWith(".")) {
      throw new Error("invalid domain to addObserver: " + domain);
    }
    if (holdWeak) {
      throw new Error("shim prefs only supports strong observers");
    }

    if (!(domain in this._observers)) {
      this._observers[domain] = [];
    }
    this._observers[domain].push(observer);
  },

  /** @see nsIPrefBranch.removeObserver */
  removeObserver: function (domain, observer) {
    if (!(domain in this._observers)) {
      return;
    }
    let index = this._observers[domain].indexOf(observer);
    if (index >= 0) {
      this._observers[domain].splice(index, 1);
    }
  },

  /** @see nsIPrefService.savePrefFile */
  savePrefFile: function (file) {
    if (file) {
      throw new Error("shim prefs only supports null file in savePrefFile");
    }
    // Nothing to do - this implementation always writes back.
  },

  /** @see nsIPrefService.getBranch */
  getBranch: function (prefRoot) {
    if (!prefRoot) {
      return this;
    }
    if (prefRoot.endsWith(".")) {
      prefRoot = prefRoot.slice(0, -1);
    }
    // This is a bit weird since it could erroneously return a pref,
    // not a pref branch.
    return this._findPref(prefRoot);
  },

  /**
   * Helper function to find either a Preference or PrefBranch object
   * given its name.  If the name is not found, throws an exception.
   *
   * @param {String} prefName the fully-qualified preference name
   * @return {Object} Either a Preference or PrefBranch object
   */
  _findPref: function (prefName) {
    let branchNames = prefName.split(".");
    let branch = this;

    for (let branchName of branchNames) {
      branch = branch._children[branchName];
      if (!branch) {
        throw new Error("could not find pref branch " + prefName);
      }
    }

    return branch;
  },

  /**
   * Helper function to notify any observers when a preference has
   * changed.  This will also notify the parent branch for further
   * reporting.
   *
   * @param {String} relativeName the name of the updated pref,
   *        relative to this branch
   */
  _notify: function (relativeName) {
    for (let domain in this._observers) {
      if (relativeName.startsWith(domain)) {
        // Allow mutation while walking.
        let localList = this._observers[domain].slice();
        for (let observer of localList) {
          try {
            observer.observe(this, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID,
                             relativeName);
          } catch (e) {
            console.error(e);
          }
        }
      }
    }

    if (this._parent) {
      this._parent._notify(this._name + "." + relativeName);
    }
  },

  /**
   * Helper function to create a branch given an array of branch names
   * representing the path of the new branch.
   *
   * @param {Array} branchList an array of strings, one per component
   *        of the branch to be created
   * @return {PrefBranch} the new branch
   */
  _createBranch: function (branchList) {
    let parent = this;
    for (let branch of branchList) {
      if (!parent._children[branch]) {
        parent._children[branch] = new PrefBranch(parent, branch,
                                                  parent.root + "." + branch);
      }
      parent = parent._children[branch];
    }
    return parent;
  },

  /**
   * Create a new preference.  The new preference is assumed to be in
   * local storage already, and the new value is taken from there.
   *
   * @param {String} keyName the full-qualified name of the preference.
   *        This is also the name of the key in local storage.
   * @param {Any} userValue the user value to use if the pref does not exist
   * @param {Any} defaultValue the default value to use if the pref
   *        does not exist
   * @param {Boolean} hasUserValue if a new pref is created, whether
   *        the default value is also a user value
   */
  _findOrCreatePref: function (keyName, userValue, hasUserValue, defaultValue) {
    let branchName = keyName.split(".");
    let prefName = branchName.pop();

    let branch = this._createBranch(branchName);
    if (!(prefName in branch._children)) {
      if (hasUserValue && typeof (userValue) !== typeof (defaultValue)) {
        throw new Error("inconsistent values when creating " + keyName);
      }

      let type;
      switch (typeof (defaultValue)) {
        case "boolean":
          type = PREF_BOOL;
          break;
        case "number":
          type = PREF_INT;
          break;
        case "string":
          type = PREF_STRING;
          break;
        default:
          throw new Error("unhandled argument type: " + typeof (defaultValue));
      }

      let thePref = new Preference(branch, prefName, keyName);
      thePref.storageUpdated(type, userValue, hasUserValue, defaultValue);
      branch._children[prefName] = thePref;
    }

    return branch._children[prefName];
  },

  /**
   * Helper function that is called when local storage changes.  This
   * updates the preferences and notifies pref observers as needed.
   *
   * @param {StorageEvent} event the event representing the local
   *        storage change
   */
  _onStorageChange: function (event) {
    if (event.storageArea !== localStorage) {
      return;
    }
    // Ignore delete events.  Not clear what's correct.
    if (event.key === null || event.newValue === null) {
      return;
    }

    let {type, userValue, hasUserValue, defaultValue} =
        JSON.parse(event.newValue);
    if (event.oldValue === null) {
      this._findOrCreatePref(event.key, userValue, hasUserValue, defaultValue);
    } else {
      let thePref = this._findPref(event.key);
      thePref.storageUpdated(type, userValue, hasUserValue, defaultValue);
    }
  },

  /**
   * Helper function to initialize the root PrefBranch.
   */
  _initializeRoot: function () {
    if (localStorage.length === 0) {
      // FIXME - this is where we'll load devtools.js to install the
      // default prefs.
    }

    // Read the prefs from local storage and create the local
    // representations.
    for (let i = 0; i < localStorage.length; ++i) {
      let keyName = localStorage.key(i);
      let {userValue, hasUserValue, defaultValue} =
          JSON.parse(localStorage.getItem(keyName));
      this._findOrCreatePref(keyName, userValue, hasUserValue, defaultValue);
    }

    this._onStorageChange = this._onStorageChange.bind(this);
    window.addEventListener("storage", this._onStorageChange);
  },
};

const Services = {
  /**
   * An implementation of nsIPrefService that is based on local
   * storage.  Only the subset of nsIPrefService that is actually used
   * by devtools is implemented here.
   */
  prefs: new PrefBranch(null, "", ""),
};

/**
 * Create a new preference.  This is used during startup (see
 * devtools/client/preferences/devtools.js) to install the
 * default preferences.
 *
 * @param {String} name the name of the preference
 * @param {Any} value the default value of the preference
 */
function pref(name, value) {
  let thePref = Services.prefs._findOrCreatePref(name, value, true, value);
  thePref.setDefault(value);
}

exports.Services = Services;
// This is exported to silence eslint and, at some point, perhaps to
// provide it when loading devtools.js in order to install the default
// preferences.
exports.pref = pref;
