/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla JavaScript Shell project.
 *
 * The Initial Developer of the Original Code is 
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alex Fritze <alex@croczilla.com>
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

 try {
   // this will only work pending bug#238324
   importModule("resource:/jscodelib/JSComponentUtils.js");
 }
 catch(e) {
var ComponentUtils = {
  generateFactory: function(ctor, interfaces) {
    return {
      createInstance: function(outer, iid) {
        if (outer) throw Components.results.NS_ERROR_NO_AGGREGATION;
        if (!interfaces)
          return ctor().QueryInterface(iid);
        for (var i=interfaces.length; i>=0; --i) {
          if (iid.equals(interfaces[i])) break;
        }
        if (i<0 && !iid.equals(Components.interfaces.nsISupports))
          throw Components.results.NS_ERROR_NO_INTERFACE;
        return ctor();
      }
    }
  },
  
  generateNSGetModule: function(classArray, postRegister, preUnregister) {
    var module = {
      getClassObject: function(compMgr, cid, iid) {
        if (!iid.equals(Components.interfaces.nsIFactory))
          throw Components.results.NS_ERROR_NO_INTERFACE;
        for (var i=0; i<classArray.length; ++i) {
          if(cid.equals(classArray[i].cid))
            return classArray[i].factory;
        }
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
      },
      registerSelf: function(compMgr, fileSpec, location, type) {
        debug("*** registering "+fileSpec.leafName+": [ ");
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        for (var i=0; i<classArray.length; ++i) {
          debug(classArray[i].className+" ");
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
        debug("*** unregistering "+fileSpec.leafName+": [ ");
        if (preUnregister)
          preUnregister(compMgr, fileSpec, classArray);
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        for (var i=0; i<classArray.length; ++i) {
          debug(classArray[i].className+" ");
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

  get categoryManager() {
    return Components.classes["@mozilla.org/categorymanager;1"]
           .getService(Components.interfaces.nsICategoryManager);
  }
};  
}

//----------------------------------------------------------------------
// JSShStarter class

// constructor
function JSShStarter() {
}

JSShStarter.prototype = {
  // nsICommandLineHandler methods:
  handle : function(commandline) {
    debug("JSShStarter: checking for -jssh startup option\n");
    if (commandline.handleFlag("jssh", false)) { 
      // start a jssh server with startupURI
      // "chrome://jssh/content/jssh-debug.js". We use 'getService'
      // instead of 'createInstance' to get a well-known, globally
      // accessible instance of a jssh-server.
      // XXX Todo: get port, startupURI and loopbackOnly from prefs.
      
      var port = commandline.handleFlagWithParam("jssh-port", false) || 9997;

      Components.classes["@mozilla.org/jssh-server;1"]
        .getService(Components.interfaces.nsIJSShServer)
        .startServerSocket(port, "chrome://jssh/content/jssh-debug.js", true);
      debug("JSShStarter: JSSh server started on port " + port + "\n");
    }
  },
  
  helpInfo : "  -jssh                Start a JSSh server on port 9997.\n" +
             "  -jssh-port <port>    Change the JSSh server port.\n",

};

//----------------------------------------------------------------------
NSGetModule = ComponentUtils.generateNSGetModule(
  [
    {
      className  : "JSShStarter",
      cid        : Components.ID("28CA200A-C070-4454-AD47-551FBAE1C48C"),
      contractID : "@mozilla.org/jssh-runner;1",
      factory    : ComponentUtils.generateFactory(function(){ return new JSShStarter();},
                                                  [Components.interfaces.nsICommandLineHandler])
    }
  ],
  function(mgr,file,arr) {
    debug("register as command-line-handler");
    ComponentUtils.categoryManager.addCategoryEntry("command-line-handler",
                                                    "a-jssh",
                                                    arr[0].contractID,
                                                    true, true);
  },
  function(mgr,file,arr) {
    debug("unregister as command-line-handler");
    ComponentUtils.categoryManager.deleteCategoryEntry("command-line-handler",
                                                       "a-jssh", true);
  }
);
