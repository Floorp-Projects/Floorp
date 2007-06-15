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
 * 'Components.utils.import("rel:XPCOMUtils.js");'
 *
 */

EXPORTED_SYMBOLS = [ "XPCOMUtils" ];

debug("*** loading XPCOMUtils\n");

var XPCOMUtils = {

  /**
   * Generate a factory object (implementing nsIFactory) for the given
   * constructor.
   *
   * By default, calls to the factory's createInstance method will
   * call the method 'QueryInterface' on the newly created object. If
   * an optional list of interfaces is provided, createInstance will
   * not call QueryInterface on newly created objects, but instead
   * check the requested iid against the interface list.
   *
   * @param ctor : A call to 'new ctor()' must return an instance of the class
   *               served by this factory.
   *
   * @param interfaces : Optional list of interfaces. If this parameter is not
   *                     given, objects created by 'ctor' must implement a
   *                     QueryInterface method. If this parameter is given,
   *                     objects do not need to implement QueryInterface; the
   *                     interface list will be consulted instead.
   */
  generateFactory: function(ctor, interfaces) {
    return {
      createInstance: function(outer, iid) {
        if (outer) throw Components.results.NS_ERROR_NO_AGGREGATION;
        if (!interfaces)
          return (new ctor()).QueryInterface(iid);
        for (var i=interfaces.length; i>=0; --i) {
          if (iid.equals(interfaces[i])) break;
        }
        if (i < 0 && !iid.equals(Components.interfaces.nsISupports))
          throw Components.results.NS_ERROR_NO_INTERFACE;
        return (new ctor());
      }
    }
  },
  
  /**
   * Generate a NSGetModule function implementation.
   *
   * @param classArray : Array of class descriptors.
   *                     A class descriptor is an
   *                     object with the following members:
   *                     {
   *                       className  : "xxx",
   *                       cid        : Components.ID("xxx"),
   *                       contractID : "@mozilla/xxx;1",
   *                       factory    : object_implementing_nsIFactory
   *                     }
   * @param postRegister  : optional post-registration function with
   *                        signature 'postRegister(nsIComponentManager,
   *                                                nsIFile,classArray)'
   * @param preUnregister : optional pre-unregistration function with
   *                        signature 'preUnregister(nsIComponentManager,
   *                                                 nsIFile,classArray)'
   */
  generateNSGetModule: function(classArray, postRegister, preUnregister) {
    var module = {
      getClassObject: function(compMgr, cid, iid) {
        if (!iid.equals(Components.interfaces.nsIFactory))
          throw Components.results.NS_ERROR_NO_INTERFACE;
        for (var i=0; i<classArray.length; ++i) {
          if (cid.equals(classArray[i].cid))
            return classArray[i].factory;
        }
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
      },
      registerSelf: function(compMgr, fileSpec, location, type) {
        debug("*** registering " + fileSpec.leafName + ": [ ");
        compMgr =
          compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        for (var i=0; i < classArray.length; ++i) {
          debug(classArray[i].className + " ");
          compMgr.registerFactoryLocation(classArray[i].cid,
                                          classArray[i].className,
                                          classArray[i].contractID,
                                          fileSpec,
                                          location,
                                          type);
        }
        if (postRegister)
          postRegister(compMgr, fileSpec, classArray);
        debug("]\n");
      },
      unregisterSelf: function(compMgr, fileSpec, location) {
        debug("*** unregistering " + fileSpec.leafName + ": [ ");
        if (preUnregister)
          preUnregister(compMgr, fileSpec, classArray);
        compMgr =
          compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        for (var i=0; i < classArray.length; ++i) {
          debug(classArray[i].className + " ");
          compMgr.unregisterFactoryLocation(classArray[i].cid, fileSpec);
        }
        debug("]\n");
      },
      canUnload: function(compMgr) {
        return true;
      }
    };

    return function(compMgr, fileSpec) {
      return module;
    }
  },

  /**
   * Convenience access to category manager
   */
  get categoryManager() {
    return Components.classes["@mozilla.org/categorymanager;1"]
           .getService(Components.interfaces.nsICategoryManager);
  }
};
