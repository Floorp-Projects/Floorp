/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
 *    Nickolay Ponomarev <asqueella@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 *    classDescription: "unique text description",
 *    classID:          Components.ID("{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"),
 *    contractID:       "@example.com/xxx;1",
 *
 *    // [optional] custom factory (an object implementing nsIFactory). If not
 *    // provided, the default factory is used, which returns
 *    // |(new MyComponent()).QueryInterface(iid)| in its createInstance().
 *    _xpcom_factory: { ... },
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
 *    // QueryInterface implementation, e.g. using the generateQI helper
 *    QueryInterface: XPCOMUtils.generateQI(
 *      [Components.interfaces.nsIObserver,
 *       Components.interfaces.nsIMyInterface,
 *       "nsIFoo",
 *       "nsIBar" ]),
 *
 *    // ...component implementation...
 *  };
 *
 * 2. Create an array of component constructors (like the one
 * created in step 1):
 *  var components = [MyComponent];
 *
 * 3. Define the NSGetModule entry point:
 *  function NSGetModule(compMgr, fileSpec) {
 *    // components is the array created in step 2.
 *    return XPCOMUtils.generateModule(components);
 *  }
 */


var EXPORTED_SYMBOLS = [ "XPCOMUtils" ];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

var XPCOMUtils = {
  /**
   * Generate a QueryInterface implementation. The returned function must be
   * assigned to the 'QueryInterface' property of a JS object. When invoked on
   * that object, it checks if the given iid is listed in the |interfaces|
   * param, and if it is, returns |this| (the object it was called on).
   */
  generateQI: function XPCU_generateQI(interfaces) {
    /* Note that Ci[Ci.x] == Ci.x for all x */
    return makeQI([Ci[i].name for each (i in interfaces) if (Ci[i])]);
  },

  /**
   * Generate a NSGetFactory function given an array of components.
   */
  generateNSGetFactory: function XPCU_generateNSGetFactory(componentsArray) {
    let classes = {};
    for each (let component in componentsArray) {
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

  get _appID() {
    try {
      let appInfo = Cc["@mozilla.org/xre/app-info;1"].
                    getService(Ci.nsIXULAppInfo);
      delete this._appID;
      return this._appID = appInfo.ID;
    }
    catch(ex) {
      return undefined;
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
    aObject.__defineGetter__(aName, function() {
      delete aObject[aName];
      return aObject[aName] = aLambda.apply(aObject);
    });
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
      return Cc[aContract].getService(Ci[aInterfaceName]);
    });
  },

  /**
   * Convenience access to category manager
   */
  get categoryManager() {
    return Components.classes["@mozilla.org/categorymanager;1"]
           .getService(Ci.nsICategoryManager);
  },

  /**
   * Helper which iterates over a nsISimpleEnumerator.
   * @param e The nsISimpleEnumerator to iterate over.
   * @param i The expected interface for each element.
   */
  IterSimpleEnumerator: function XPCU_IterSimpleEnumerator(e, i)
  {
    while (e.hasMoreElements())
      yield e.getNext().QueryInterface(i);
  },

  /**
   * Helper which iterates over a string enumerator.
   * @param e The string enumerator (nsIUTF8StringEnumerator or
   *          nsIStringEnumerator) over which to iterate.
   */
  IterStringEnumerator: function XPCU_IterStringEnumerator(e)
  {
    while (e.hasMore())
      yield e.getNext();
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
        }
      }
    }
    return factory;
  }
};

/**
 * Helper for XPCOMUtils.generateQI to avoid leaks - see bug 381651#c1
 */
function makeQI(interfaceNames) {
  return function XPCOMUtils_QueryInterface(iid) {
    if (iid.equals(Ci.nsISupports))
      return this;
    for each(let interfaceName in interfaceNames) {
      if (Ci[interfaceName].equals(iid))
        return this;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  };
}

