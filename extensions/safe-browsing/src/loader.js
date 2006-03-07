/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Boodman <aa@google.com> (original author)
 *   Fritz Schneider <fritz@google.com>
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


// We run our application in a separate context from the browser. Our
// app gets its own context by virtue of the fact that it is an XPCOM
// component. Glue code in the overlay will call into our component
// to interact with the app.
//
// This file implements a loader for our component. It takes a list of
// js files and dynamically loads them into this context. It then
// registers to hear profile afterchange, at which point it
// instantiates our application in this context. This context is
// exposed to overlay code via a property of the object returned when
// you get the service from contract
// @mozilla.org/safebrowsing/application;1.
//
// Lots of JS and XPCOM voodoo going on here; tread lightly unless you
// understand it.
//
// TODO: delete category entry for startup on shutdown?
// TODO: null out refs when unneeded?
// TODO: catch window closes so we can null out refs?
// TODO: 
// 
// HISTORICAL NOTE: this file was cloned from an early version of Aaron's
// bootstrap loader at a time when we were trying to figure out how this
// technique should work. As a result, this code is uglier than it needs
// to be. We should replace this loader with the newer, more elegent 
// version that Aaron has (jsmodule.js).

var G_GDEBUG_LOADER = false;

var Cc = Components.classes;
var Ci = Components.interfaces;

var PROT_appContext = this;
var PROT_application;

// Directory under extension root in which our library is found
var PROT_LIB_ROOT = "lib";

// All the js files are specified here. Path is relative to lib/ under
// the extension root. Note that ORDER HERE IS IMPORTANT! JS files
// with side effects (e.g., instantiation) need to be loaded after
// their dependencies.
var PROT_LIBS = [
    "js/lang.js", 
    "js/thread-queue.js",
    "js/eventregistrar.js",
    "js/listdictionary.js",
    "js/arc4.js",

    "moz/lang.js",
    "moz/preferences.js",
    "moz/alarm.js",
    "moz/base64.js",
    "moz/observer.js",
    "moz/filesystem.js",
    "moz/protocol4.js",
    "moz/debug.js",
    "moz/tabbedbrowserwatcher.js",
    "moz/navwatcher.js",
    "moz/cryptohasher.js",
    "moz/objectsafemap.js",
    "moz/version-utils.js",

    "map.js",
    "url-canonicalizer.js",
    "enchash-decrypter.js",
    "trtable.js",
    "firefox-commands.js",
    "url-crypto-key-manager.js",
    "url-crypto.js",
    "xml-fetcher.js",
    "tr-fetcher.js",
    "reporter.js",
    "wireformat.js",
    "globalstore.js",
    "browser-view.js",
    "phishing-afterload-displayer.js",
    "phishing-warden.js",
    "listmanager.js",
    "controller.js",
    "application.js",
    ];

var G_GDEBUG = false;

/**
 * We usually use G_Debug, but we have a bootstrap problem with this
 * object, since it is the one responsible for loading G_Debug.
 */
function LOADER_dump(msg) {
  if (G_GDEBUG_LOADER) {

    for (var i = 1; i < arguments.length; i++) {
      msg = msg.replace(/\%s/, arguments[i]);
    }

    dump("[TB BOOTSTRAP LOADER] " + msg + "\n");
  }
}

/**
 * SB_loader is our singleton bootstrap loader. It gets loaded at
 * startup as an XPCOM service because it is in the components
 * directory. It loads the files specified above into its context,
 * giving us a nice, clean, and flat namespace to work in, much like a
 * browser window does for a web application. Applications then get
 * this loader as a service, and call into its scope using the 
 * .appContext property.
 */
var SB_loader = new function() {
  var CONTRACT_ID_PREFIX = "@mozilla.org/safebrowsing/";
  var LOADER_CONTRACT_ID = "application;1";

  var components = [];
  var componentsLookup = {};

  this.init = function() {

    LOADER_dump("initializing...");
    loadLibs();
    
    LOADER_dump("registering loader service...");

    // Register ourselves so that glue can find us
    this.registerComponent(LOADER_CONTRACT_ID, 
                           "{de437057-05f8-4ad4-81bd-e2005d64eff3}",
                           "SB_loader", 
                           ["xpcom-startup"]);
  };
              
              
  /**
   * Arranges for a global javascript object to be registered as an
   * XPCOM component.
   *
   * Usage:
   *   SB_loader.registerComponent("mycomponent;1", 
   *                                "{...}",
   *                                "MyComponent",
   *                                ["app-startup", "xpcom-startup"]);
   */
  this.registerComponent = 
    function(contractId, classId, className, categories) {
      var component = { contractId: contractId,
                        classId: classId,
                        className: className,
                        categories: categories };
      
      components.push(component);
      componentsLookup[component.classId] = component;
    };


  // nsIModule
  this.registerSelf = function(compMgr, fileSpec, location, type) {
    LOADER_dump("registering XPCOM components... ");
    compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
    
    for (var i = 0, component; component = components[i]; i++) {
      LOADER_dump("  registering {%s} as {%s}", 
                  component.className, 
                  component.contractId);

      compMgr.registerFactoryLocation(Components.ID(component.classId),
                                      component.className,
                                      CONTRACT_ID_PREFIX + 
                                      component.contractId,
                                      fileSpec,
                                      location,
                                      type);
      
      if (component.categories && component.categories.length) {
        var catMgr = Cc["@mozilla.org/categorymanager;1"]
                       .getService(Ci.nsICategoryManager);

        for (var j = 0, category; category = component.categories[j]; j++) {
          LOADER_dump("    registering category {%s}", category);
          catMgr.addCategoryEntry(category,
                                  component.className,
                                  CONTRACT_ID_PREFIX + component.contractId,
                                  true,
                                  true);
        }
      }
    }
  };

  this.getClassObject = function(compMgr, cid, iid) {
    LOADER_dump("getting factory for class {%s} and interface {%s}...", 
                cid, iid);
    
    var comp = componentsLookup[cid.toString()];
    
    if (!comp)
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    if (!iid.equals(Ci.nsIFactory))
      throw Components.results.NS_ERROR_NO_INTERFACE;

    return new PROT_Factory(comp);
  };

  this.canUnload = function(compMgr) {
    return true;
  };


  /**
   * Load all the libraries into this context
   */
  function loadLibs() {
    for (var i = 0, libPath; libPath = PROT_LIBS[i]; i++) {
      LOADER_dump("Loading library {%s}", libPath);
      Cc["@mozilla.org/moz/jssubscript-loader;1"]
        .getService(Ci.mozIJSSubScriptLoader)
        .loadSubScript(getLibUrl(libPath));
    }
  };

  /**
   * Gets a file:// URL for the given physical path relative to the libs/ 
   * folder. 
   */
  function getLibUrl(path) {
    var file = __LOCATION__.clone().parent.parent;
    var parts = path.split("/");

    file.append(PROT_LIB_ROOT);

    for (var i = 0, part; part = parts[i]; i++)
      file.append(part);
    
    if (!file.exists())
      throw new Error("Specified library {" + file.path + "} does not exist");

    return Cc["@mozilla.org/network/protocol;1?name=file"]
      .getService(Ci.nsIFileProtocolHandler)
      .getURLSpecFromFile(file);
  };


  // The following stuff is machinery necessary to enabled the glue code
  // in the overlay to access this context.
 
  // Expose the underlying SB_loader object, not a wrapped version
  this.__defineGetter__("wrappedJSObject", function() { return this; });

  // This context is exposed through this property
  this.appContext = PROT_appContext;

  // We observe the xpcom-startup category and then wait for profile-
  // after-change to actually start up. We have to wait until that
  // late because we want to know whether the SafeBrowsing extension
  // is installed before instantiating our app, and to know that the
  // user's profile needs to be known.
  this.observe = function(subject, topic, data) {
    if (topic != "xpcom-startup")
      return;

    function onProfileAfterChange() {
      if (PROT_Application.isCompatibleWithThisFirefox()) {
        LOADER_dump("Starting application...");
        PROT_application = new PROT_Application();
      } else 
        LOADER_dump("Not starting application (incompatible ffox)...");
    };
    
    // We (maybe) run our initialization when they have a profile
    new G_ObserverServiceObserver("profile-after-change", 
                                  onProfileAfterChange, 
                                  true /* unregister after observing */);
  };

  // XPCOM cruft
  this.QueryInterface = function(iid) {
    var Ci = Components.interfaces;
    if (iid.equals(Ci.nsISupports) || iid.equals(Ci.nsIObserver))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  };
}


/**
 * Implements nsIFactory for the javascript objects we register as xpcom 
 * components.
 */
function PROT_Factory(comp) {
  this.comp = comp;
}

PROT_Factory.global = this;

// nsIFactory
PROT_Factory.prototype.createInstance = function(outer, iid) {
  LOADER_dump("Factory creating instance for iid {%s}", iid);
  
  if (outer != null)
    throw Components.results.NS_ERROR_NO_AGGREGATION;
  
  // Look for the global object with that name in the service's global context
  var scriptObj = PROT_Factory.global[this.comp.className];

  if (!scriptObj)
    throw new Error("Could not find global object with name {%s}", 
                    this.comp.className);

  // Little convenience here. If the object referenced is a function
  // we presume it is meant to be used as a constructor and return an
  // instance of it.
  var retVal = (typeof scriptObj == "function") ? new scriptObj() : scriptObj;

  // If the js object has QueryInterface, call it with the target iid
  // for extra precaution. Otherwise, we follow the js pattern and
  // assume that it implements the interface.
  if (retVal.QueryInterface) {
    retVal.QueryInterface(iid);
  }

  return retVal;
}

SB_loader.init();
LOADER_dump("initialization complete");

function NSGetModule() {
  LOADER_dump("NSGetModule called");
  return SB_loader;
}
