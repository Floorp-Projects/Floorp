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
 * The Original Code is Mozilla Seamonkey Composer.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <bsmedberg@covad.net>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

const nsICmdLineHandler     = Components.interfaces.nsICmdLineHandler;
const nsIFactory            = Components.interfaces.nsIFactory;
const nsISupports           = Components.interfaces.nsISupports;
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;
const nsICategoryManager    = Components.interfaces.nsICategoryManager;

const NS_ERROR_FAILURE        = Components.results.NS_ERROR_FAILURE;
const NS_ERROR_NO_AGGREGATION = Components.results.NS_ERROR_NO_AGGREGATION;
const NS_ERROR_NO_INTERFACE   = Components.results.NS_ERROR_NO_INTERFACE;

function nsComposerCmdLineHandler() {
}

nsComposerCmdLineHandler.prototype = {
  /* nsISupports */

  QueryInterface: function(iid) {
    if (!iid.equals(nsICmdLineHandler) &&
        !iid.equals(nsISupports)) {
          throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  },

  /* nsICmdLineHandler */

  get commandLineArgument() {
    return "-edit";
  },

  get prefNameForStartup() {
    return "general.startup.editor";
  },

  get chromeUrlForTask() {
    return "chrome://editor/content/editor.xul";
  },

  get helpText() {
    return "Start with editor."
  },

  get handlesArgs() {
    return true;
  },

  get defaultArgs() {
    return "about:blank";
  },

  get openWindowWithArgs() {
    return true;
  }
};

function nsComposerCmdLineHandlerFactory() {
}

nsComposerCmdLineHandlerFactory.prototype = {
  /* nsISupports */

  QueryInterface: function(iid) {
    if (!iid.equals(nsIFactory) &&
        !iid.equals(nsISupports)) {
          throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  },

  /* nsIFactory */

  createInstance: function(outer, iid) {
    if (outer != null) {
      throw NS_ERROR_NO_AGGREGATION;
    }

    return new nsComposerCmdLineHandler().QueryInterface(iid);
  },

  lockFactory: function(lock) {
  }
};

const nsComposerCmdLineHandler_CID =
  Components.ID("{f7d8db95-ab5d-4393-a796-9112fe758cfa}");

const ContractIDPrefix =
  "@mozilla.org/commandlinehandler/general-startup;1?type=";

var thisModule = {
  /* nsISupports */

  QueryInterface: function(iid) {
    if (!iid.equals(nsIModule) &&
        !iid.equals(nsISupports)) {
          throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  },

  /* nsIModule */

  getClassObject: function (compMgr, cid, iid) {
    if (!cid.equals(nsComposerCmdLineHandler_CID)) {
      throw NS_ERROR_FAILURE;
    }

    if (!iid.equals(nsIFactory)) {
      throw NS_ERROR_NO_INTERFACE;
    }

    return new nsComposerCmdLineHandlerFactory();
  },

  registerSelf: function (compMgr, fileSpec, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);
    compReg.registerFactoryLocation(nsComposerCmdLineHandler_CID,
                                    "nsComposerCmdLineHandler",
                                    ContractIDPrefix + "edit",
                                    fileSpec, location, type);
    compReg.registerFactoryLocation(nsComposerCmdLineHandler_CID,
                                    "nsComposerCmdLineHandler",
                                    ContractIDPrefix + "editor",
                                    fileSpec, location, type);

    var catMan = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
    catMan.addCategoryEntry("command-line-argument-handlers",
                            "nsComposerCmdLineHandler",
                            ContractIDPrefix + "edit",
                            true, true);
  },

  unregisterSelf: function (compMgr, location, type) {
    var compReg = compMgr.QueryInterface(nsIComponentRegistrar);
    compReg.unregisterFactoryLocation(nsComposerCmdLineHandler_CID,
                                      location);

    var catMan = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
    catMan.deleteCategoryEntry("command-line-argument-handlers",
                               "nsComposerCmdLineHandler", true);
  },    

  canUnload: function (compMgr) {
    return true;
  }
};

function NSGetModule(compMgr, fileSpec) {
  return thisModule;
}
