/*
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
 *      service: true
 *    }],
 *
 *    // QueryInterface implementation, e.g. using the generateQI helper
 *    QueryInterface: XPCOMUtils.generateQI(
 *      [Components.interfaces.nsIObserver,
 *       Components.interfaces.nsIMyInterface]),
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

const Ci = Components.interfaces;
const Cr = Components.results;

var XPCOMUtils = {
  /**
   * Generate a QueryInterface implementation. The returned function must be
   * assigned to the 'QueryInterface' property of a JS object. When invoked on
   * that object, it checks if the given iid is listed in the |interfaces|
   * param, and if it is, returns |this| (the object it was called on).
   */
  generateQI: function(interfaces) {
    return makeQI([i.name for each(i in interfaces)]);
  },

  /**
   * Generate the NSGetModule function (along with the module definition).
   * See the parameters to generateModule.
   */
  generateNSGetModule: function(componentsArray, postRegister, preUnregister) {
    return function NSGetModule(compMgr, fileSpec) {
      return XPCOMUtils.generateModule(componentsArray,
                                       postRegister,
                                       preUnregister);
    }
  },

  /**
   * Generate a module implementation.
   *
   * @param componentsArray  Array of component constructors. See the comment
   *                         at the top of this file for details.
   * @param postRegister  optional post-registration function with
   *                      signature 'postRegister(nsIComponentManager,
   *                                              nsIFile, componentsArray)'
   * @param preUnregister optional pre-unregistration function with
   *                      signature 'preUnregister(nsIComponentManager,
   *                                               nsIFile, componentsArray)'
   */
  generateModule: function(componentsArray, postRegister, preUnregister) {
    let classes = [];
    for each (let component in componentsArray) {
      classes.push({
        cid:          component.prototype.classID,
        className:    component.prototype.classDescription,
        contractID:   component.prototype.contractID,
        factory:      this._getFactory(component),
        categories:   component.prototype._xpcom_categories
      });
    }

    return { // nsIModule impl.
      getClassObject: function(compMgr, cid, iid) {
        // We only support nsIFactory queries, not nsIClassInfo
        if (!iid.equals(Ci.nsIFactory))
          throw Cr.NS_ERROR_NOT_IMPLEMENTED;

        for each (let classDesc in classes) {
          if (classDesc.cid.equals(cid))
            return classDesc.factory;
        }

        throw Cr.NS_ERROR_FACTORY_NOT_REGISTERED;
      },

      registerSelf: function(compMgr, fileSpec, location, type) {
        var componentCount = 0;
        debug("*** registering " + fileSpec.leafName + ": [ ");
        compMgr.QueryInterface(Ci.nsIComponentRegistrar);

        for each (let classDesc in classes) {
          debug((componentCount++ ? ", " : "") + classDesc.className);
          compMgr.registerFactoryLocation(classDesc.cid,
                                          classDesc.className,
                                          classDesc.contractID,
                                          fileSpec,
                                          location,
                                          type);
          if (classDesc.categories) {
            let catMan = XPCOMUtils.categoryManager;
            for each (let cat in classDesc.categories) {
              let defaultValue = (cat.service ? "service," : "") +
                                 classDesc.contractID;
              catMan.addCategoryEntry(cat.category,
                                      cat.entry || classDesc.className,
                                      cat.value || defaultValue,
                                      true, true);
            }
          }
        }

        if (postRegister)
          postRegister(compMgr, fileSpec, componentsArray);
        debug(" ]\n");
      },

      unregisterSelf: function(compMgr, fileSpec, location) {
        var componentCount = 0;
        debug("*** unregistering " + fileSpec.leafName + ": [ ");
        compMgr.QueryInterface(Ci.nsIComponentRegistrar);
        if (preUnregister)
          preUnregister(compMgr, fileSpec, componentsArray);

        for each (let classDesc in classes) {
          debug((componentCount++ ? ", " : "") + classDesc.className);
          if (classDesc.categories) {
            let catMan = XPCOMUtils.categoryManager;
            for each (let cat in classDesc.categories) {
              catMan.deleteCategoryEntry(cat.category,
                                         cat.entry || classDesc.className,
                                         true);
            }
          }
          compMgr.unregisterFactoryLocation(classDesc.cid, fileSpec);
        }
        debug(" ]\n");
      },

      canUnload: function(compMgr) {
        return true;
      }
    };
  },

  /**
   * Convenience access to category manager
   */
  get categoryManager() {
    return Components.classes["@mozilla.org/categorymanager;1"]
           .getService(Ci.nsICategoryManager);
  },

  /**
   * Returns an nsIFactory for |component|.
   */
  _getFactory: function(component) {
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
