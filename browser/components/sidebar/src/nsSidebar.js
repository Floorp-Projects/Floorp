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
 * Contributor(s): Stephen Lamm            <slamm@netscape.com>
 *                 Robert John Churchill   <rjc@netscape.com>
 *                 David Hyatt             <hyatt@mozilla.org>
 *                 Christopher A. Aillon   <christopher@aillon.com>
 */

/*
 * No magic constructor behaviour, as is de rigeur for XPCOM.
 * If you must perform some initialization, and it could possibly fail (even
 * due to an out-of-memory condition), you should use an Init method, which
 * can convey failure appropriately (thrown exception in JS,
 * NS_FAILED(nsresult) return in C++).
 *
 * In JS, you can actually cheat, because a thrown exception will cause the
 * CreateInstance call to fail in turn, but not all languages are so lucky.
 * (Though ANSI C++ provides exceptions, they are verboten in Mozilla code
 * for portability reasons -- and even when you're building completely
 * platform-specific code, you can't throw across an XPCOM method boundary.)
 */

const DEBUG = false; /* set to false to suppress debug messages */

const SIDEBAR_CONTRACTID        = "@mozilla.org/sidebar;1";
const SIDEBAR_CID               = Components.ID("{22117140-9c6e-11d3-aaf1-00805f8a4905}");
const NETSEARCH_CONTRACTID      = "@mozilla.org/rdf/datasource;1?name=internetsearch"
const nsISupports               = Components.interfaces.nsISupports;
const nsIFactory                = Components.interfaces.nsIFactory;
const nsISidebar                = Components.interfaces.nsISidebar;
const nsIInternetSearchService  = Components.interfaces.nsIInternetSearchService;
const nsIClassInfo              = Components.interfaces.nsIClassInfo;

function nsSidebar()
{
    const PROMPTSERVICE_CONTRACTID = "@mozilla.org/embedcomp/prompt-service;1";
    const nsIPromptService = Components.interfaces.nsIPromptService;
    this.promptService =
        Components.classes[PROMPTSERVICE_CONTRACTID].getService(nsIPromptService);
}

nsSidebar.prototype.nc = "http://home.netscape.com/NC-rdf#";

function sidebarURLSecurityCheck(url)
{
    if (url.search(/(^http:|^ftp:|^https:)/) == -1)
        throw "Script attempted to add sidebar panel from illegal source";
}

/* decorate prototype to provide ``class'' methods and property accessors */
nsSidebar.prototype.addPanel =
function (aTitle, aContentURL, aCustomizeURL)
{
    debug("addPanel(" + aTitle + ", " + aContentURL + ", " +
          aCustomizeURL + ")");
   
    return this.addPanelInternal(aTitle, aContentURL, aCustomizeURL, false);
}

nsSidebar.prototype.addPersistentPanel = 
function(aTitle, aContentURL, aCustomizeURL)
{
    debug("addPersistentPanel(" + aTitle + ", " + aContentURL + ", " +
           aCustomizeURL + ")\n");

    return this.addPanelInternal(aTitle, aContentURL, aCustomizeURL, true);
}

nsSidebar.prototype.addPanelInternal =
function (aTitle, aContentURL, aCustomizeURL, aPersist)
{
    var WINMEDSVC = Components.classes['@mozilla.org/appshell/window-mediator;1']
                              .getService(Components.interfaces.nsIWindowMediator);
    var win = WINMEDSVC.getMostRecentWindow( "navigator:browser" );
                                                                                
    sidebarURLSecurityCheck(aContentURL);
    win.openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
                   "centerscreen,chrome,dialog,resizable,dependent",
                   aTitle, aContentURL, null, null, null, null, true);
}

/* decorate prototype to provide ``class'' methods and property accessors */
nsSidebar.prototype.addSearchEngine =
function (engineURL, iconURL, suggestedTitle, suggestedCategory)
{
    debug("addSearchEngine(" + engineURL + ", " + iconURL + ", " +
          suggestedCategory + ", " + suggestedTitle + ")");

    try
    {
        // make sure using HTTP (for both engine as well as icon URLs)

        if (engineURL.search(/^http:\/\//i) == -1)
        {
            debug ("must use HTTP to fetch search engine file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

        if (iconURL.search(/^http:\/\//i) == -1)
        {
            debug ("must use HTTP to fetch search icon file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

        // make sure engineURL refers to a .src file
        if (engineURL.search(/\.src$/i) == -1)
        {
            debug ("engineURL doesn't reference a .src file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

        // make sure iconURL refers to a .gif/.jpg/.jpeg/.png file
        if (iconURL.search(/\.(gif|jpg|jpeg|png)$/i) == -1)
        {
            debug ("iconURL doesn't reference a supported image file");
            throw Components.results.NS_ERROR_INVALID_ARG;
        }

    }
    catch(ex)
    {
        this.promptService.alert(null, "Failed to add the search engine.");
        throw Components.results.NS_ERROR_INVALID_ARG;
    }

    var titleMessage, dialogMessage;
    try {
        var stringBundle = srGetStrBundle("chrome://browser/locale/sidebar/sidebar.properties");
        if (stringBundle) {
            titleMessage = stringBundle.GetStringFromName("addEngineConfirmTitle");
            dialogMessage = stringBundle.GetStringFromName("addEngineConfirmMessage");
            dialogMessage = dialogMessage.replace(/%title%/, suggestedTitle);
            dialogMessage = dialogMessage.replace(/%category%/, suggestedCategory);
            dialogMessage = dialogMessage.replace(/%url%/, engineURL);
            dialogMessage = dialogMessage.replace(/#/g, "\n");
        }
    }
    catch (e) {
        titleMessage = "Add Search Engine";
        dialogMessage = "Add the following search engine?\n\nName: " + suggestedTitle;
        dialogMessage += "\nSearch Category: " + suggestedCategory;
        dialogMessage += "\nSource: " + engineURL;
    }
          
    var rv = this.promptService.confirm(null, titleMessage, dialogMessage);
      
    if (!rv)
        return;

    var internetSearch = Components.classes[NETSEARCH_CONTRACTID].getService();
    if (internetSearch)    
        internetSearch = internetSearch.QueryInterface(nsIInternetSearchService);
    if (internetSearch)
    {
        internetSearch.AddSearchEngine(engineURL, iconURL, suggestedTitle,
                                       suggestedCategory);
    }
}

// property of nsIClassInfo
nsSidebar.prototype.flags = nsIClassInfo.DOM_OBJECT;

// property of nsIClassInfo
nsSidebar.prototype.classDescription = "Sidebar";

// method of nsIClassInfo
nsSidebar.prototype.getInterfaces = function(count) {
    var interfaceList = [nsISidebar, nsIClassInfo];
    count.value = interfaceList.length;
    return interfaceList;
}

// method of nsIClassInfo
nsSidebar.prototype.getHelperForLanguage = function(count) {return null;}

nsSidebar.prototype.QueryInterface =
function (iid) {
    if (!iid.equals(nsISidebar) && 
        !iid.equals(nsIClassInfo) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
}

var sidebarModule = new Object();

sidebarModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    debug("registering (all right -- a JavaScript module!)");
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(SIDEBAR_CID, 
                                    "Sidebar JS Component",
                                    SIDEBAR_CONTRACTID, 
                                    fileSpec, 
                                    location,
                                    type);
    const CATMAN_CONTRACTID = "@mozilla.org/categorymanager;1";
    const nsICategoryManager = Components.interfaces.nsICategoryManager;
    var catman = Components.classes[CATMAN_CONTRACTID].
                            getService(nsICategoryManager);
                            
    const JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY = "JavaScript global property";
    catman.addCategoryEntry(JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
                            "sidebar",
                            SIDEBAR_CONTRACTID,
                            true,
                            true);
}

sidebarModule.getClassObject =
function (compMgr, cid, iid) {
    if (!cid.equals(SIDEBAR_CID))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    
    return sidebarFactory;
}

sidebarModule.canUnload =
function(compMgr)
{
    debug("Unloading component.");
    return true;
}
    
/* factory object */
var sidebarFactory = new Object();

sidebarFactory.createInstance =
function (outer, iid) {
    debug("CI: " + iid);
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    return (new nsSidebar()).QueryInterface(iid);
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return sidebarModule;
}

/* static functions */
if (DEBUG)
    debug = function (s) { dump("-*- sidebar component: " + s + "\n"); }
else
    debug = function (s) {}

var strBundleService = null;
function srGetStrBundle(path)
{
   var strBundle = null;
   if (!strBundleService) {
       try {
          strBundleService =
          Components.classes["@mozilla.org/intl/stringbundle;1"].getService(); 
          strBundleService = 
          strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
       } catch (ex) {
          dump("\n--** strBundleService failed: " + ex + "\n");
          return null;
      }
   }
   strBundle = strBundleService.createBundle(path); 
   if (!strBundle) {
       dump("\n--** strBundle createInstance failed **--\n");
   }
   return strBundle;
}
