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


const CHATZILLASERVICE_PROGID = "component://netscape/commandlinehandler/general-startup-chat";
const CHATZILLASERVICE_CID = Components.ID("{38a95514-1dd2-11b2-97e7-9da958640f2c}");
const nsICmdLineHandler = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsISupports      = Components.interfaces.nsISupports;


function ChatzillaService()
{
	dump("in ChatzillaService()\n");
}

ChatzillaService.prototype.commandLineArgument = "-chat";
ChatzillaService.prototype.prefNameForStartup = "general.startup.chat";
ChatzillaService.prototype.chromeUrlForTask="chrome://chatzilla/content";
ChatzillaService.prototype.helpText = "Start with an IRC chat client";
ChatzillaService.prototype.handlesArgs=false;
ChatzillaService.prototype.defaultArgs ="";
ChatzillaService.prototype.openWindowWithArgs=false;

var chatzillaModule = new Object();

chatzillaModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    dump("registering (all right -- a JavaScript module!)");
    compMgr.registerComponentWithType(CHATZILLASERVICE_CID, "Chatzilla Service Component",
                                      CHATZILLASERVICE_PROGID, fileSpec, location,
                                      true, true, type);

	catman = Components.classes["mozilla.categorymanager.1"].getService(nsICategoryManager);
	catman.addCategoryEntry("command-line-argument-handlers",CHATZILLASERVICE_PROGID, "chatzilla command line handler", true, true);
}

chatzillaModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
    compMgr.unregisterComponentSpec(CHATZILLASERVICE_CID, fileSpec);
	catman = Components.classes["mozilla.categorymanager.1"].getService(nsICategoryManager);
	catman.deleteCategoryEntry("command-line-argument-handlers",CHATZILLASERVICE_PROGID, true);
}

chatzillaModule.getClassObject =
function (compMgr, cid, iid) {
    if (!cid.equals(CHATZILLASERVICE_CID))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    
    return chatzillaFactory;
}

chatzillaModule.canUnload =
function(compMgr)
{
    dump("Unloading component.");
    return true;
}
    
/* factory object */
chatzillaFactory = new Object();

chatzillaFactory.CreateInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsICmdLineHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new ChatzillaService();
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return chatzillaModule;
}
