/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


/* components defined in this file */
const SMIME_EXTENSION_SERVICE_CONTRACTID =
    "@mozilla.org/accounmanager/extension;1?name=smime";
const SMIME_EXTENSION_SERVICE_CID =
    Components.ID("{f2809796-1dd1-11b2-8c1b-8f15f007c699}");

/* interafces used in this file */
const nsIMsgAccountManagerExtension  = Components.interfaces.nsIMsgAccountManagerExtension;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsISupports        = Components.interfaces.nsISupports;

function SMIMEService()
{}

SMIMEService.prototype.name = "smime";
SMIMEService.prototype.chromePackageName = "messenger";
SMIMEService.prototype.showPanel =

function (server)

{
  // don't show the S/MIME panel for news accounts
  return (server.type != "nntp");
}

/* factory for command line handler service (SMIMEService) */
var SMIMEFactory = new Object();

SMIMEFactory.createInstance =
function (outer, iid) {
  if (outer != null)
    throw Components.results.NS_ERROR_NO_AGGREGATION;

  if (!iid.equals(nsIMsgAccountManagerExtension) && !iid.equals(nsISupports))
    throw Components.results.NS_ERROR_INVALID_ARG;

  return new SMIMEService();
}


var SMIMEModule = new Object();

SMIMEModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
  debug("*** Registering smime account manager extension.\n");
  compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(SMIME_EXTENSION_SERVICE_CID,
                                  "SMIME Account Manager Extension Service",
                                  SMIME_EXTENSION_SERVICE_CONTRACTID, 
                                  fileSpec,
                                  location, 
                                  type);
  catman = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
  catman.addCategoryEntry("mailnews-accountmanager-extensions",
                            "smime account manager extension",
                            SMIME_EXTENSION_SERVICE_CONTRACTID, true, true);
}

SMIMEModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
  compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  compMgr.unregisterFactoryLocation(SMIME_EXTENSION_SERVICE_CID, fileSpec);
  catman = Components.classes["@mozilla.org/categorymanager;1"].getService(nsICategoryManager);
  catman.deleteCategoryEntry("mailnews-accountmanager-extensions",
                             SMIME_EXTENSION_SERVICE_CONTRACTID, true);
}

SMIMEModule.getClassObject =
function (compMgr, cid, iid) {
  if (cid.equals(SMIME_EXTENSION_SERVICE_CID))
    return SMIMEFactory;


  if (!iid.equals(Components.interfaces.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  throw Components.results.NS_ERROR_NO_INTERFACE;    
}

SMIMEModule.canUnload =
function(compMgr)
{
  return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
  return SMIMEModule;
}


