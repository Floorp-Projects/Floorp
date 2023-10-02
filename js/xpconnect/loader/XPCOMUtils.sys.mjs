/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

let global = Cu.getGlobalForObject({});

// Some global imports expose additional symbols; for example,
// `Cu.importGlobalProperties(["MessageChannel"])` imports `MessageChannel`
// and `MessagePort`. This table maps those extra symbols to the main
// import name.
const EXTRA_GLOBAL_NAME_TO_IMPORT_NAME = {
  MessagePort: "MessageChannel",
};

/**
 * Redefines the given property on the given object with the given
 * value. This can be used to redefine getter properties which do not
 * implement setters.
 */
function redefine(object, prop, value) {
  Object.defineProperty(object, prop, {
    configurable: true,
    enumerable: true,
    value,
    writable: true,
  });
  return value;
}

export var XPCOMUtils = {
  /**
   * Defines a getter on a specified object that will be created upon first use.
   *
   * @param aObject
   *        The object to define the lazy getter on.
   * @param aName
   *        The name of the getter to define on aObject.
   * @param aLambda
   *        A function that returns what the getter should return.  This will
   *        only ever be called once.
   */
  defineLazyGetter(aObject, aName, aLambda) {
    ChromeUtils.defineLazyGetter(aObject, aName, aLambda);
  },

  /**
   * Defines a getter on a specified object for a script.  The script will not
   * be loaded until first use.
   *
   * @param aObject
   *        The object to define the lazy getter on.
   * @param aNames
   *        The name of the getter to define on aObject for the script.
   *        This can be a string if the script exports only one symbol,
   *        or an array of strings if the script can be first accessed
   *        from several different symbols.
   * @param aResource
   *        The URL used to obtain the script.
   */
  defineLazyScriptGetter(aObject, aNames, aResource) {
    if (!Array.isArray(aNames)) {
      aNames = [aNames];
    }
    for (let name of aNames) {
      Object.defineProperty(aObject, name, {
        get() {
          XPCOMUtils._scriptloader.loadSubScript(aResource, aObject);
          return aObject[name];
        },
        set(value) {
          redefine(aObject, name, value);
        },
        configurable: true,
        enumerable: true,
      });
    }
  },

  /**
   * Overrides the scriptloader definition for tests to help with globals
   * tracking. Should only be used for tests.
   *
   * @param {object} aObject
   *        The alternative script loader object to use.
   */
  overrideScriptLoaderForTests(aObject) {
    Cu.crashIfNotInAutomation();
    delete this._scriptloader;
    this._scriptloader = aObject;
  },

  /**
   * Defines a getter property on the given object for each of the given
   * global names as accepted by Cu.importGlobalProperties. These
   * properties are imported into the shared JSM module global, and then
   * copied onto the given object, no matter which global the object
   * belongs to.
   *
   * @param {object} aObject
   *        The object on which to define the properties.
   * @param {string[]} aNames
   *        The list of global properties to define.
   */
  defineLazyGlobalGetters(aObject, aNames) {
    for (let name of aNames) {
      this.defineLazyGetter(aObject, name, () => {
        if (!(name in global)) {
          let importName = EXTRA_GLOBAL_NAME_TO_IMPORT_NAME[name] || name;
          // eslint-disable-next-line mozilla/reject-importGlobalProperties, no-unused-vars
          Cu.importGlobalProperties([importName]);
        }
        return global[name];
      });
    }
  },

  /**
   * Defines a getter on a specified object for a service.  The service will not
   * be obtained until first use.
   *
   * @param aObject
   *        The object to define the lazy getter on.
   * @param aName
   *        The name of the getter to define on aObject for the service.
   * @param aContract
   *        The contract used to obtain the service.
   * @param aInterfaceName
   *        The name of the interface to query the service to.
   */
  defineLazyServiceGetter(aObject, aName, aContract, aInterfaceName) {
    this.defineLazyGetter(aObject, aName, () => {
      if (aInterfaceName) {
        return Cc[aContract].getService(Ci[aInterfaceName]);
      }
      return Cc[aContract].getService().wrappedJSObject;
    });
  },

  /**
   * Defines a lazy service getter on a specified object for each
   * property in the given object.
   *
   * @param aObject
   *        The object to define the lazy getter on.
   * @param aServices
   *        An object with a property for each service to be
   *        imported, where the property name is the name of the
   *        symbol to define, and the value is a 1 or 2 element array
   *        containing the contract ID and, optionally, the interface
   *        name of the service, as passed to defineLazyServiceGetter.
   */
  defineLazyServiceGetters(aObject, aServices) {
    for (let [name, service] of Object.entries(aServices)) {
      // Note: This is hot code, and cross-compartment array wrappers
      // are not JIT-friendly to destructuring or spread operators, so
      // we need to use indexed access instead.
      this.defineLazyServiceGetter(
        aObject,
        name,
        service[0],
        service[1] || null
      );
    }
  },

  /**
   * Defines a lazy module getter on a specified object for each
   * property in the given object.
   *
   * @param aObject
   *        The object to define the lazy getter on.
   * @param aModules
   *        An object with a property for each module property to be
   *        imported, where the property name is the name of the
   *        imported symbol and the value is the module URI.
   */
  defineLazyModuleGetters(aObject, aModules) {
    for (let [name, module] of Object.entries(aModules)) {
      ChromeUtils.defineModuleGetter(aObject, name, module);
    }
  },

  /**
   * Defines a getter on a specified object for preference value. The
   * preference is read the first time that the property is accessed,
   * and is thereafter kept up-to-date using a preference observer.
   *
   * @param aObject
   *        The object to define the lazy getter on.
   * @param aName
   *        The name of the getter property to define on aObject.
   * @param aPreference
   *        The name of the preference to read.
   * @param aDefaultPrefValue
   *        The default value to use, if the preference is not defined.
   *        This is the default value of the pref, before applying aTransform.
   * @param aOnUpdate
   *        A function to call upon update. Receives as arguments
   *         `(aPreference, previousValue, newValue)`
   * @param aTransform
   *        An optional function to transform the value.  If provided,
   *        this function receives the new preference value as an argument
   *        and its return value is used by the getter.
   */
  defineLazyPreferenceGetter(
    aObject,
    aName,
    aPreference,
    aDefaultPrefValue = null,
    aOnUpdate = null,
    aTransform = val => val
  ) {
    if (AppConstants.DEBUG && aDefaultPrefValue !== null) {
      let prefType = Services.prefs.getPrefType(aPreference);
      if (prefType != Ci.nsIPrefBranch.PREF_INVALID) {
        // The pref may get defined after the lazy getter is called
        // at which point the code here won't know the expected type.
        let prefTypeForDefaultValue = {
          boolean: Ci.nsIPrefBranch.PREF_BOOL,
          number: Ci.nsIPrefBranch.PREF_INT,
          string: Ci.nsIPrefBranch.PREF_STRING,
        }[typeof aDefaultPrefValue];
        if (prefTypeForDefaultValue != prefType) {
          throw new Error(
            `Default value does not match preference type (Got ${prefTypeForDefaultValue}, expected ${prefType}) for ${aPreference}`
          );
        }
      }
    }

    // Note: We need to keep a reference to this observer alive as long
    // as aObject is alive. This means that all of our getters need to
    // explicitly close over the variable that holds the object, and we
    // cannot define a value in place of a getter after we read the
    // preference.
    let observer = {
      QueryInterface: XPCU_lazyPreferenceObserverQI,

      value: undefined,

      observe(subject, topic, data) {
        if (data == aPreference) {
          if (aOnUpdate) {
            let previous = this.value;

            // Fetch and cache value.
            this.value = undefined;
            let latest = lazyGetter();
            aOnUpdate(data, previous, latest);
          } else {
            // Empty cache, next call to the getter will cause refetch.
            this.value = undefined;
          }
        }
      },
    };

    let defineGetter = get => {
      Object.defineProperty(aObject, aName, {
        configurable: true,
        enumerable: true,
        get,
      });
    };

    function lazyGetter() {
      if (observer.value === undefined) {
        let prefValue;
        switch (Services.prefs.getPrefType(aPreference)) {
          case Ci.nsIPrefBranch.PREF_STRING:
            prefValue = Services.prefs.getStringPref(aPreference);
            break;

          case Ci.nsIPrefBranch.PREF_INT:
            prefValue = Services.prefs.getIntPref(aPreference);
            break;

          case Ci.nsIPrefBranch.PREF_BOOL:
            prefValue = Services.prefs.getBoolPref(aPreference);
            break;

          case Ci.nsIPrefBranch.PREF_INVALID:
            prefValue = aDefaultPrefValue;
            break;

          default:
            // This should never happen.
            throw new Error(
              `Error getting pref ${aPreference}; its value's type is ` +
                `${Services.prefs.getPrefType(aPreference)}, which I don't ` +
                `know how to handle.`
            );
        }

        observer.value = aTransform(prefValue);
      }
      return observer.value;
    }

    defineGetter(() => {
      Services.prefs.addObserver(aPreference, observer, true);

      defineGetter(lazyGetter);
      return lazyGetter();
    });
  },

  /**
   * Defines a non-writable property on an object.
   */
  defineConstant(aObj, aName, aValue) {
    Object.defineProperty(aObj, aName, {
      value: aValue,
      enumerable: true,
      writable: false,
    });
  },
};

ChromeUtils.defineLazyGetter(XPCOMUtils, "_scriptloader", () => {
  return Services.scriptloader;
});

var XPCU_lazyPreferenceObserverQI = ChromeUtils.generateQI([
  "nsIObserver",
  "nsISupportsWeakReference",
]);
