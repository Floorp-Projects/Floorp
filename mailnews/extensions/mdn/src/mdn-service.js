/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 * Seth Spitzer <sspitzer@netscape.com>
 */


/* components defined in this file */
const MDN_EXTENSION_SERVICE_CONTRACTID =
    "@mozilla.org/accounmanager/extension;1?name=mdn";
const MDN_EXTENSION_SERVICE_CID =
    Components.ID("{e007d92e-1dd1-11b2-a61e-dc962c9b8571}");

/* interafces used in this file */
const nsIMsgAccountManagerExtension  = Components.interfaces.nsIMsgAccountManagerExtension;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsISupports        = Components.interfaces.nsISupports;

function MDNService()
{}

MDNService.prototype.name = "mdn";
MDNService.prototype.chromePackageName = "messenger";
MDNService.prototype.showPanel =

function (server)

{
  // don't show the panel for news accounts
  return (server.type != "nntp");
}

/* factory for command line handler service (MDNService) */
var MDNFactory = new Object();

MDNFactory.createInstance =
function (outer, iid) {
  if (outer != null)
    throw Components.results.NS_ERROR_NO_AGGREGATION;

  if (!iid.equals(nsIMsgAccountManagerExtension) && !iid.equals(nsISupports))
    throw Components.results.NS_ERROR_INVALID_ARG;

  return new MDNService();
}


var MDNModule = new Object();

MDNModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
  debug("*** Registering mdn account manager extension.\n");
  compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(MDN_EXTENSION_SERVICE_CID,
                                  "MDN Account Manager Extension Service",
                                  MDN_EXTENSION_SERVICE_CONTRACTID, 
                                  fileSpec,
                                  location, 
                                  type);
  catman = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
  catman.addCategoryEntry("mailnews-accountmanager-extensions",
                            "mdn account manager extension",
                            MDN_EXTENSION_SERVICE_CONTRACTID, true, true);
}

MDNModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
  compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  compMgr.unregisterFactoryLocation(MDN_EXTENSION_SERVICE_CID, fileSpec);
  catman = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
  catman.deleteCategoryEntry("mailnews-accountmanager-extensions",
                             MDN_EXTENSION_SERVICE_CONTRACTID, true);
}

MDNModule.getClassObject =
function (compMgr, cid, iid) {
  if (cid.equals(MDN_EXTENSION_SERVICE_CID))
    return MDNFactory;


  if (!iid.equals(Components.interfaces.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  throw Components.results.NS_ERROR_NO_INTERFACE;    
}

MDNModule.canUnload =
function(compMgr)
{
  return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
  return MDNModule;
}


