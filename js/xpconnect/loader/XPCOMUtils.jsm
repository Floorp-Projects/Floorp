/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Utilities for JavaScript components loaded by the JS component
 * loader.
 *
 * Import into a JS component using
 * 'Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");'
 *
 * Exposing a JS 'class' as a component using these utility methods consists
 * of several steps:
 * 0. Import XPCOMUtils, as described above.
 * 1. Declare the 'class' (or multiple classes) implementing the component(s):
 *  function MyComponent() {
 *    // constructor
 *  }
 *  MyComponent.prototype = {
 *    // properties required for XPCOM registration:
 *    classID:          Components.ID("{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"),
 *
 *    // [optional] custom factory (an object implementing nsIFactory). If not
 *    // provided, the default factory is used, which returns
 *    // |(new MyComponent()).QueryInterface(iid)| in its createInstance().
 *    _xpcom_factory: { ... },
 *
 *    // QueryInterface implementation, e.g. using the generateQI helper
 *    QueryInterface: XPCOMUtils.generateQI(
 *      [Components.interfaces.nsIObserver,
 *       Components.interfaces.nsIMyInterface,
 *       "nsIFoo",
 *       "nsIBar" ]),
 *
 *    // [optional] classInfo implementation, e.g. using the generateCI helper.
 *    // Will be automatically returned from QueryInterface if that was
 *    // generated with the generateQI helper.
 *    classInfo: XPCOMUtils.generateCI(
 *      {classID: Components.ID("{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"),
 *       contractID: "@example.com/xxx;1",
 *       classDescription: "unique text description",
 *       interfaces: [Components.interfaces.nsIObserver,
 *                    Components.interfaces.nsIMyInterface,
 *                    "nsIFoo",
 *                    "nsIBar"],
 *       flags: Ci.nsIClassInfo.SINGLETON}),
 *
 *    // The following properties were used prior to Mozilla 2, but are no
 *    // longer supported. They may still be included for compatibility with
 *    // prior versions of XPCOMUtils. In Mozilla 2, this information is
 *    // included in the .manifest file which registers this JS component.
 *    classDescription: "unique text description",
 *    contractID:       "@example.com/xxx;1",
 *
 *    // [optional] an array of categories to register this component in.
 *    _xpcom_categories: [{
 *      // Each object in the array specifies the parameters to pass to
 *      // nsICategoryManager.addCategoryEntry(). 'true' is passed for
 *      // both aPersist and aReplace params.
 *      category: "some-category",
 *      // optional, defaults to the object's classDescription
 *      entry: "entry name",
 *      // optional, defaults to the object's contractID (unless
 *      // 'service' is specified)
 *      value: "...",
 *      // optional, defaults to false. When set to true, and only if 'value'
 *      // is not specified, the concatenation of the string "service," and the
 *      // object's contractID is passed as aValue parameter of addCategoryEntry.
 *      service: true,
 *      // optional, it can be an array of applications' IDs in the form:
 *      // [ "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}", ... ]
 *      // If defined the component will be registered in this category only for
 *      // the provided applications.
 *      apps: [...]
 *    }],
 *
 *    // ...component implementation...
 *  };
 *
 * 2. Create an array of component constructors (like the one
 * created in step 1):
 *  var components = [MyComponent];
 *
 * 3. Define the NSGetFactory entry point:
 *  this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
 */


var EXPORTED_SYMBOLS = [ "XPCOMUtils" ];

var XPCOMUtils = {
  /**
   * Generate a QueryInterface implementation. The returned function must be
   * assigned to the 'QueryInterface' property of a JS object. When invoked on
   * that object, it checks if the given iid is listed in the |interfaces|
   * param, and if it is, returns |this| (the object it was called on).
   * If the JS object has a classInfo property it'll be returned for the
   * nsIClassInfo IID, generateCI can be used to generate the classInfo
   * property.
   */
  generateQI: function XPCU_generateQI(interfaces) {
    return ChromeUtils.generateQI(interfaces);
  },

  /**
   * Generate a ClassInfo implementation for a component. The returned object
   * must be assigned to the 'classInfo' property of a JS object. The first and
   * only argument should be an object that contains a number of optional
   * properties: "interfaces", "contractID", "classDescription", "classID" and
   * "flags". The values of the properties will be returned as the values of the
   * various properties of the nsIClassInfo implementation.
   */
  generateCI: function XPCU_generateCI(classInfo)
  {
    if (QueryInterface in classInfo)
      throw Error("In generateCI, don't use a component for generating classInfo");
    /* Note that Ci[Ci.x] == Ci.x for all x */
    let _interfaces = [];
    for (let i = 0; i < classInfo.interfaces.length; i++) {
      let iface = classInfo.interfaces[i];
      if (Ci[iface]) {
        _interfaces.push(Ci[iface]);
      }
    }
    return {
      getInterfaces: function XPCU_getInterfaces(countRef) {
        countRef.value = _interfaces.length;
        return _interfaces;
      },
      getScriptableHelper: function XPCU_getScriptableHelper() {
        return null;
      },
      contractID: classInfo.contractID,
      classDescription: classInfo.classDescription,
      classID: classInfo.classID,
      flags: classInfo.flags,
      QueryInterface: this.generateQI([Ci.nsIClassInfo])
    };
  },

  /**
   * Generate a NSGetFactory function given an array of components.
   */
  generateNSGetFactory: function XPCU_generateNSGetFactory(componentsArray) {
    let classes = {};
    for (let i = 0; i < componentsArray.length; i++) {
        let component = componentsArray[i];
        if (!(component.prototype.classID instanceof Components.ID))
          throw Error("In generateNSGetFactory, classID missing or incorrect for component " + component);

        classes[component.prototype.classID] = this._getFactory(component);
    }
    return function NSGetFactory(cid) {
      let cidstring = cid.toString();
      if (cidstring in classes)
        return classes[cidstring];
      throw Cr.NS_ERROR_FACTORY_NOT_REGISTERED;
    }
  },

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
  defineLazyGetter: function XPCU_defineLazyGetter(aObject, aName, aLambda)
  {
    Object.defineProperty(aObject, aName, {
      get: function () {
        // Redefine this accessor property as a data property.
        // Delete it first, to rule out "too much recursion" in case aObject is
        // a proxy whose defineProperty handler might unwittingly trigger this
        // getter again.
        delete aObject[aName];
        let value = aLambda.apply(aObject);
        Object.defineProperty(aObject, aName, {
          value,
          writable: true,
          configurable: true,
          enumerable: true
        });
        return value;
      },
      configurable: true,
      enumerable: true
    });
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
  defineLazyScriptGetter: function XPCU_defineLazyScriptGetter(aObject, aNames,
                                                               aResource)
  {
    if (!Array.isArray(aNames)) {
      aNames = [aNames];
    }
    for (let name of aNames) {
      Object.defineProperty(aObject, name, {
        get: function() {
          for (let n of aNames) {
            delete aObject[n];
          }
          Services.scriptloader.loadSubScript(aResource, aObject);
          return aObject[name];
        },
        configurable: true,
        enumerable: true
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
  defineLazyServiceGetter: function XPCU_defineLazyServiceGetter(aObject, aName,
                                                                 aContract,
                                                                 aInterfaceName)
  {
    this.defineLazyGetter(aObject, aName, function XPCU_serviceLambda() {
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
  defineLazyServiceGetters: function XPCU_defineLazyServiceGetters(
                                   aObject, aServices)
  {
    for (let [name, service] of Object.entries(aServices)) {
      // Note: This is hot code, and cross-compartment array wrappers
      // are not JIT-friendly to destructuring or spread operators, so
      // we need to use indexed access instead.
      this.defineLazyServiceGetter(aObject, name, service[0], service[1] || null);
    }
  },

  /**
   * Defines a getter on a specified object for a module.  The module will not
   * be imported until first use. The getter allows to execute setup and
   * teardown code (e.g.  to register/unregister to services) and accepts
   * a proxy object which acts on behalf of the module until it is imported.
   *
   * @param aObject
   *        The object to define the lazy getter on.
   * @param aName
   *        The name of the getter to define on aObject for the module.
   * @param aResource
   *        The URL used to obtain the module.
   * @param aSymbol
   *        The name of the symbol exported by the module.
   *        This parameter is optional and defaults to aName.
   * @param aPreLambda
   *        A function that is executed when the proxy is set up.
   *        This will only ever be called once.
   * @param aPostLambda
   *        A function that is executed when the module has been imported to
   *        run optional teardown procedures on the proxy object.
   *        This will only ever be called once.
   * @param aProxy
   *        An object which acts on behalf of the module to be imported until
   *        the module has been imported.
   */
  defineLazyModuleGetter: function XPCU_defineLazyModuleGetter(
                                   aObject, aName, aResource, aSymbol,
                                   aPreLambda, aPostLambda, aProxy)
  {
    if (arguments.length == 3) {
      return ChromeUtils.defineModuleGetter(aObject, aName, aResource);
    }

    let proxy = aProxy || {};

    if (typeof(aPreLambda) === "function") {
      aPreLambda.apply(proxy);
    }

    this.defineLazyGetter(aObject, aName, function XPCU_moduleLambda() {
      var temp = {};
      try {
        ChromeUtils.import(aResource, temp);

        if (typeof(aPostLambda) === "function") {
          aPostLambda.apply(proxy);
        }
      } catch (ex) {
        Cu.reportError("Failed to load module " + aResource + ".");
        throw ex;
      }
      return temp[aSymbol || aName];
    });
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
  defineLazyModuleGetters: function XPCU_defineLazyModuleGetters(
                                   aObject, aModules)
  {
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
   * @param aDefaultValue
   *        The default value to use, if the preference is not defined.
   * @param aOnUpdate
   *        A function to call upon update. Receives as arguments
   *         `(aPreference, previousValue, newValue)`
   * @param aTransform
   *        An optional function to transform the value.  If provided,
   *        this function receives the new preference value as an argument
   *        and its return value is used by the getter.
   */
  defineLazyPreferenceGetter: function XPCU_defineLazyPreferenceGetter(
                                   aObject, aName, aPreference,
                                   aDefaultValue = null,
                                   aOnUpdate = null,
                                   aTransform = val => val)
  {
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
    }

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
            prefValue = aDefaultValue;
            break;

          default:
            // This should never happen.
            throw new Error(`Error getting pref ${aPreference}; its value's type is ` +
                            `${Services.prefs.getPrefType(aPreference)}, which I don't ` +
                            `know how to handle.`);
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
   * Helper which iterates over a nsISimpleEnumerator.
   * @param e The nsISimpleEnumerator to iterate over.
   * @param i The expected interface for each element.
   */
  IterSimpleEnumerator: function* XPCU_IterSimpleEnumerator(e, i)
  {
    while (e.hasMoreElements())
      yield e.getNext().QueryInterface(i);
  },

  /**
   * Helper which iterates over a string enumerator.
   * @param e The string enumerator (nsIUTF8StringEnumerator or
   *          nsIStringEnumerator) over which to iterate.
   */
  IterStringEnumerator: function* XPCU_IterStringEnumerator(e)
  {
    while (e.hasMore())
      yield e.getNext();
  },

  /**
   * Helper which iterates over the entries in a category.
   * @param aCategory The name of the category over which to iterate.
   */
  enumerateCategoryEntries: function* XPCOMUtils_enumerateCategoryEntries(aCategory)
  {
    let category = this.categoryManager.enumerateCategory(aCategory);
    for (let entry of this.IterSimpleEnumerator(category, Ci.nsISupportsCString)) {
      yield [entry.data, this.categoryManager.getCategoryEntry(aCategory, entry.data)];
    }
  },

  /**
   * Returns an nsIFactory for |component|.
   */
  _getFactory: function XPCOMUtils__getFactory(component) {
    var factory = component.prototype._xpcom_factory;
    if (!factory) {
      factory = {
        createInstance: function(outer, iid) {
          if (outer)
            throw Cr.NS_ERROR_NO_AGGREGATION;
          return (new component()).QueryInterface(iid);
        },
        QueryInterface: ChromeUtils.generateQI([Ci.nsIFactory])
      }
    }
    return factory;
  },

  /**
   * Allows you to fake a relative import. Expects the global object from the
   * module that's calling us, and the relative filename that we wish to import.
   */
  importRelative: function XPCOMUtils__importRelative(that, path, scope) {
    if (!("__URI__" in that))
      throw Error("importRelative may only be used from a JSM, and its first argument "+
                  "must be that JSM's global object (hint: use this)");

    Cu.importGlobalProperties(["URL"]);
    ChromeUtils.import(new URL(path, that.__URI__).href, scope || that);
  },

  /**
   * generates a singleton nsIFactory implementation that can be used as
   * the _xpcom_factory of the component.
   * @param aServiceConstructor
   *        Constructor function of the component.
   */
  generateSingletonFactory:
  function XPCOMUtils_generateSingletonFactory(aServiceConstructor) {
    return {
      _instance: null,
      createInstance: function XPCU_SF_createInstance(aOuter, aIID) {
        if (aOuter !== null) {
          throw Cr.NS_ERROR_NO_AGGREGATION;
        }
        if (this._instance === null) {
          this._instance = new aServiceConstructor();
        }
        return this._instance.QueryInterface(aIID);
      },
      lockFactory: function XPCU_SF_lockFactory(aDoLock) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
      },
      QueryInterface: ChromeUtils.generateQI([Ci.nsIFactory])
    };
  },

  /**
   * Defines a non-writable property on an object.
   */
  defineConstant: function XPCOMUtils__defineConstant(aObj, aName, aValue) {
    Object.defineProperty(aObj, aName, {
      value: aValue,
      enumerable: true,
      writable: false
    });
  },
};

var XPCU_lazyPreferenceObserverQI = ChromeUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]);

ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(XPCOMUtils, "categoryManager",
                                   "@mozilla.org/categorymanager;1",
                                   "nsICategoryManager");
