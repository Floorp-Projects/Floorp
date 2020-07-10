/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Deprecated utilities for JavaScript components loaded by the JS component
 * loader.
 *
 * Import into a JS component using
 * 'Components.utils.import("resource://gre/modules/ComponentUtils.jsm");'
 *
 * Exposing a JS 'class' as a component using these utility methods consists
 * of several steps:
 * 0. Import ComponentUtils, as described above.
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
 *    QueryInterface: ChromeUtils.generateQI(
 *      [Components.interfaces.nsIObserver,
 *       Components.interfaces.nsIMyInterface,
 *       "nsIFoo",
 *       "nsIBar" ]),
 *
 *    // The following properties were used prior to Mozilla 2, but are no
 *    // longer supported. They may still be included for compatibility with
 *    // prior versions of ComponentUtils. In Mozilla 2, this information is
 *    // included in the .manifest file which registers this JS component.
 *    classDescription: "unique text description",
 *    contractID:       "@example.com/xxx;1",
 *
 *    // ...component implementation...
 *  };
 *
 * 2. Create an array of component constructors (like the one
 * created in step 1):
 *  var components = [MyComponent];
 *
 * 3. Define the NSGetFactory entry point:
 *  this.NSGetFactory = ComponentUtils.generateNSGetFactory(components);
 */

var EXPORTED_SYMBOLS = ["ComponentUtils"];

const nsIFactoryQI = ChromeUtils.generateQI(["nsIFactory"]);

var ComponentUtils = {
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
        QueryInterface: nsIFactoryQI
      }
    }
    return factory;
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
      QueryInterface: nsIFactoryQI
    };
  },
};
