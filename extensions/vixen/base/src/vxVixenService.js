/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */

const COMMANDLINE_SVC_CONTRACTID = "@mozilla.org/commandlinehandler/general-startup;1?type=vixen";
const COMMANDLINE_SVC_CID = Components.ID("{37a95514-1dd2-11b2-97e7-9da958640f2c}");

const nsICmdLineHandler = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsISupports = Components.interfaces.nsISupports;

function CommandLineService()
{
}

CommandLineService.prototype = {
  commandLineArgument: "-vixen",
  prefNameForStartup:  "general.startup.vixen",
  chromeUrlForTask:    "chrome://vixen/content",
  helpText:            "Start the Visual XUL Environment",
  handleArgs:          false, // XXX - support project/document parameters
  defaultArgs:         "",
  openWindowWithArgs:  false
};

var CommandLineFactory = {
  createInstance: function (aOuter, aIID)
  {
    if (aOuter)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
     
    if (!aIID.equals(nsICmdLineHandler) && 
        !aIID.equals(nsISupports)) 
      throw Components.results.NS_ERROR_INVALID_ARG;

    return new CommandLineService();
  }
}

var JSVixenModule = { };

JSVixenModule = {
  registerSelf: function (aCompMgr, aFileSpec, aLocation, aType) 
  {
    aCompMgr.registerComponentWithType(COMMANDLINE_SVC_CID, 
                                       "Vixen Command Line Service", 
                                       COMMANDLINE_SVC_CONTRACTID, aFileSpec,
                                       aLocation, true, true, aType);
    var catMan = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
    catMan.addCategoryEntry("command-line-argument-handlers",
                            "Vixen Command Line Service",
                            COMMANDLINE_SVC_CONTRACTID, true, true);
  },

  unregisterSelf: function (aCompMgr, aFileSpec, aLocation)
  {
    aCompMgr.unregisterComponentSpec(COMMANDLINE_SVC_CID, aFileSpec);
    var catMan = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
    catMan.deleteCategoryEntry("command-line-argument-handlers", 
                               COMMANDLINE_SVC_CONTRACTID, true);
  },

  getClassObject: function (aCompMgr, aCID, aIID) 
  { 
    if (aCID.equals(COMMANDLINE_SVC_CID)) return CommandLineFactory;
    if (!aCID.equals(Components.interfaces.nsIFactory)) 
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  canUnload: function (aCompMgr)
  { 
    return true;
  }
};

function NSGetModule(aCompMgr, aFileSpec) 
{
  return JSVixenModule;
}


