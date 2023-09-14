/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");

/**
 * Shortcuts for lazily accessing and setting various preferences.
 * Usage:
 *   let prefs = new Prefs("root.path.to.branch", {
 *     myIntPref: ["Int", "leaf.path.to.my-int-pref"],
 *     myCharPref: ["Char", "leaf.path.to.my-char-pref"],
 *     myJsonPref: ["Json", "leaf.path.to.my-json-pref"],
 *     myFloatPref: ["Float", "leaf.path.to.my-float-pref"]
 *     ...
 *   });
 *
 * Get/set:
 *   prefs.myCharPref = "foo";
 *   let aux = prefs.myCharPref;
 *
 * Observe:
 *   prefs.registerObserver();
 *   prefs.on("pref-changed", (prefValue) => {
 *     ...
 *   });
 *
 * @param string prefsRoot
 *        The root path to the required preferences branch.
 * @param object prefsBlueprint
 *        An object containing { accessorName: [prefType, prefName] } keys.
 */
function PrefsHelper(prefsRoot = "", prefsBlueprint = {}) {
  EventEmitter.decorate(this);

  const cache = new Map();

  for (const accessorName in prefsBlueprint) {
    const [prefType, prefName, fallbackValue] = prefsBlueprint[accessorName];
    map(
      this,
      cache,
      accessorName,
      prefType,
      prefsRoot,
      prefName,
      fallbackValue
    );
  }

  const observer = makeObserver(this, cache, prefsRoot, prefsBlueprint);
  this.registerObserver = () => observer.register();
  this.unregisterObserver = () => observer.unregister();
}

/**
 * Helper method for getting a pref value.
 *
 * @param Map cache
 * @param string prefType
 * @param string prefsRoot
 * @param string prefName
 * @param string|int|boolean fallbackValue
 * @return any
 */
function get(cache, prefType, prefsRoot, prefName, fallbackValue) {
  const cachedPref = cache.get(prefName);
  if (cachedPref !== undefined) {
    return cachedPref;
  }
  const value = Services.prefs["get" + prefType + "Pref"](
    [prefsRoot, prefName].join("."),
    fallbackValue
  );
  cache.set(prefName, value);
  return value;
}

/**
 * Helper method for setting a pref value.
 *
 * @param Map cache
 * @param string prefType
 * @param string prefsRoot
 * @param string prefName
 * @param any value
 */
function set(cache, prefType, prefsRoot, prefName, value) {
  Services.prefs["set" + prefType + "Pref"](
    [prefsRoot, prefName].join("."),
    value
  );
  cache.set(prefName, value);
}

/**
 * Maps a property name to a pref, defining lazy getters and setters.
 * Supported types are "Bool", "Char", "Int", "Float" (sugar around "Char"
 * type and casting), and "Json" (which is basically just sugar for "Char"
 * using the standard JSON serializer).
 *
 * @param PrefsHelper self
 * @param Map cache
 * @param string accessorName
 * @param string prefType
 * @param string prefsRoot
 * @param string prefName
 * @param string|int|boolean fallbackValue
 * @param array serializer [optional]
 */
function map(
  self,
  cache,
  accessorName,
  prefType,
  prefsRoot,
  prefName,
  fallbackValue,
  serializer = { in: e => e, out: e => e }
) {
  if (prefName in self) {
    throw new Error(
      `Can't use ${prefName} because it overrides a property` +
        "on the instance."
    );
  }
  if (prefType == "Json") {
    map(
      self,
      cache,
      accessorName,
      "String",
      prefsRoot,
      prefName,
      fallbackValue,
      {
        in: JSON.parse,
        out: JSON.stringify,
      }
    );
    return;
  }
  if (prefType == "Float") {
    map(self, cache, accessorName, "Char", prefsRoot, prefName, fallbackValue, {
      in: Number.parseFloat,
      out: n => n + "",
    });
    return;
  }

  Object.defineProperty(self, accessorName, {
    get: () =>
      serializer.in(get(cache, prefType, prefsRoot, prefName, fallbackValue)),
    set: e => {
      set(cache, prefType, prefsRoot, prefName, serializer.out(e));
    },
  });
}

/**
 * Finds the accessor for the provided pref, based on the blueprint object
 * used in the constructor.
 *
 * @param PrefsHelper self
 * @param object prefsBlueprint
 * @return string
 */
function accessorNameForPref(somePrefName, prefsBlueprint) {
  for (const accessorName in prefsBlueprint) {
    const [, prefName] = prefsBlueprint[accessorName];
    if (somePrefName == prefName) {
      return accessorName;
    }
  }
  return "";
}

/**
 * Creates a pref observer for `self`.
 *
 * @param PrefsHelper self
 * @param Map cache
 * @param string prefsRoot
 * @param object prefsBlueprint
 * @return object
 */
function makeObserver(self, cache, prefsRoot, prefsBlueprint) {
  return {
    register() {
      this._branch = Services.prefs.getBranch(prefsRoot + ".");
      this._branch.addObserver("", this);
    },
    unregister() {
      this._branch.removeObserver("", this);
    },
    observe(subject, topic, prefName) {
      // If this particular pref isn't handled by the blueprint object,
      // even though it's in the specified branch, ignore it.
      const accessorName = accessorNameForPref(prefName, prefsBlueprint);
      if (!(accessorName in self)) {
        return;
      }
      cache.delete(prefName);
      self.emit("pref-changed", accessorName, self[accessorName]);
    },
  };
}

exports.PrefsHelper = PrefsHelper;

/**
 * A PreferenceObserver observes a pref branch for pref changes.
 * It emits an event for each preference change.
 */
class PrefObserver extends EventEmitter {
  constructor(branchName) {
    super();

    this.#branchName = branchName;
    this.#branch = Services.prefs.getBranch(branchName);
    this.#branch.addObserver("", this);
  }

  #branchName;
  #branch;

  observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      this.emit(this.#branchName + data);
    }
  }

  destroy() {
    this.#branch.removeObserver("", this);
  }
}

exports.PrefObserver = PrefObserver;
