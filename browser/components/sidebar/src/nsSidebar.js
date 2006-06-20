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
 *   Stephen Lamm            <slamm@netscape.com>
 *   Robert John Churchill   <rjc@netscape.com>
 *   David Hyatt             <hyatt@mozilla.org>
 *   Christopher A. Aillon   <christopher@aillon.com>
 *   Myk Melez               <myk@mozilla.org>
 *   Pamela Greene           <pamg.bugs@gmail.com>
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
const nsISupports               = Components.interfaces.nsISupports;
const nsIFactory                = Components.interfaces.nsIFactory;
const nsISidebar                = Components.interfaces.nsISidebar;
const nsISidebarExternal        = Components.interfaces.nsISidebarExternal;
const nsIClassInfo              = Components.interfaces.nsIClassInfo;

function nsSidebar()
{
    const PROMPTSERVICE_CONTRACTID = "@mozilla.org/embedcomp/prompt-service;1";
    const nsIPromptService = Components.interfaces.nsIPromptService;
    this.promptService =
        Components.classes[PROMPTSERVICE_CONTRACTID].getService(nsIPromptService);

    const SEARCHSERVICE_CONTRACTID = "@mozilla.org/browser/search-service;1";
    const nsIBrowserSearchService = Components.interfaces.nsIBrowserSearchService;
    this.searchService =
      Components.classes[SEARCHSERVICE_CONTRACTID].getService(nsIBrowserSearchService);
}

nsSidebar.prototype.nc = "http://home.netscape.com/NC-rdf#";

function sidebarURLSecurityCheck(url)
{
    if (!/^(https?:|ftp:)/i.test(url)) {
        Components.utils.reportError("Invalid argument passed to window.sidebar.addPanel: Unsupported panel URL." );
        return false;
    }
    return true;
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
                                                                                
    if (!sidebarURLSecurityCheck(aContentURL))
      return;

    var dialogArgs = {
      name: aTitle,
      url: aContentURL,
      bWebPanel: true
    }
    var features;
    if (!/Mac/.test(win.navigator.platform))
      features = "centerscreen,chrome,dialog,resizable,dependent";
    else
      features = "chrome,dialog,resizable,modal";
    win.openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
                   features, dialogArgs);
}

nsSidebar.prototype.confirmSearchEngine =
function (engineURL, suggestedTitle)
{
  var stringBundle = srGetStrBundle("chrome://browser/locale/sidebar/sidebar.properties");
  var titleMessage = stringBundle.GetStringFromName("addEngineConfirmTitle");

  // With tentative title: Add "Somebody's Search" to the list...
  // With no title:        Add a new search engine to the list...
  var engineName;
  if (suggestedTitle)
    engineName = stringBundle.formatStringFromName("addEngineQuotedEngineName",
                                                   [suggestedTitle], 1);
  else
    engineName = stringBundle.GetStringFromName("addEngineDefaultEngineName");

  // Display only the hostname portion of the URL.
  try {
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);
    var uri = ios.newURI(engineURL, null, null);
    engineURL = uri.host;
  }
  catch (e) {
    // If newURI fails, fall back to the full URL.
  }

  var dialogMessage = stringBundle.formatStringFromName("addEngineConfirmText",
                                                        [engineName, engineURL], 2);
  var checkboxMessage = stringBundle.GetStringFromName("addEngineUseNowText");
  var addButtonLabel = stringBundle.GetStringFromName("addEngineAddButtonLabel");

  const ps = this.promptService;
  var buttonFlags = (ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0) +
                    (ps.BUTTON_TITLE_CANCEL    * ps.BUTTON_POS_1) +
                     ps.BUTTON_POS_0_DEFAULT;

  var checked = {value: false};
  // confirmEx returns the index of the button that was pressed.  Since "Add" is
  // button 0, we want to return the negation of that value.
  var confirm = !this.promptService.confirmEx(null,
                                         titleMessage,
                                         dialogMessage,
                                         buttonFlags,
                                         addButtonLabel,
                                         "", "", // button 1 & 2 names not used
                                         checkboxMessage,
                                         checked);

  return {confirmed: confirm, useNow: checked.value};
}

// The expected suffix for the engine URL should be bare, i.e. without the
// leading dot.  "src" (for Sherlock files) and "xml" (for OpenSearch files)
// are common choices.
nsSidebar.prototype.validateSearchEngine =
function (engineURL, expectedSuffix, iconURL)
{
  try
  {
    // make sure using HTTP, HTTPS, or FTP and refering to the expected kind of file
    // for the engine.
    const engineRegexp = new RegExp("^(https?|ftp):\/\/.+\." + expectedSuffix + "$", "i");
    if (! engineRegexp.test(engineURL))
      throw "Unsupported search engine URL";
  
    // make sure using HTTP, HTTPS, or FTP and refering to a
    // .gif/.jpg/.jpeg/.png/.ico file for the icon.
    if (iconURL &&
        ! /^(https?|ftp):\/\/.+\.(gif|jpg|jpeg|png|ico)$/i.test(iconURL))
      throw "Unsupported search icon URL.";
  }
  catch(ex)
  {
    debug(ex);
    Components.utils.reportError("Invalid argument passed to window.sidebar.addSearchEngine: " + ex);
    return false;
  }
  
  return true;
}

nsSidebar.prototype.addSearchEngine =
function (engineURL, iconURL, suggestedTitle, suggestedCategory)
{
  debug("addSearchEngine(" + engineURL + ", " + iconURL + ", " +
        suggestedCategory + ", " + suggestedTitle + ")");

  if (!this.validateSearchEngine(engineURL, "src", iconURL))
    return;

  var confirmation = this.confirmSearchEngine(engineURL, suggestedTitle);
  if (!confirmation.confirmed)
    return;

  const typeText = Components.interfaces.nsISearchEngine.DATA_TEXT;
  this.searchService.addEngine(engineURL, typeText, iconURL,
                               confirmation.useNow);
}

// This function exists largely to implement window.external.AddSearchProvider(),
// to match other browsers' APIs.  The capitalization, although nonstandard here,
// is therefore important.
nsSidebar.prototype.AddSearchProvider =
function (aDescriptionURL)
{
  // Get the favicon URL for the current page, or our best guess at the current
  // page since we don't have easy access to the active document.  Most search
  // engines will override this with an icon specified in the OpenSearch
  // description anyway.
  var WINMEDSVC = Components.classes['@mozilla.org/appshell/window-mediator;1']
                            .getService(Components.interfaces.nsIWindowMediator);
  var win = WINMEDSVC.getMostRecentWindow("navigator:browser");
  var browser = win.document.getElementById("content");
  var iconURL = "";
  if (browser.shouldLoadFavIcon(browser.selectedBrowser.currentURI))
    iconURL = win.gProxyFavIcon.getAttribute("src");
  
  if (!this.validateSearchEngine(aDescriptionURL, "xml", iconURL))
    return;

  var confirmation = this.confirmSearchEngine(aDescriptionURL, "");
  if (!confirmation.confirmed)
    return;

  const typeXML = Components.interfaces.nsISearchEngine.DATA_XML;
  this.searchService.addEngine(aDescriptionURL, typeXML, iconURL,
                               confirmation.useNow);
}

nsSidebar.prototype.addMicrosummaryGenerator =
function (generatorURL)
{
    debug("addMicrosummaryGenerator(" + generatorURL + ")");

    var stringBundle = srGetStrBundle("chrome://browser/locale/sidebar/sidebar.properties");
    var titleMessage = stringBundle.GetStringFromName("addMicsumGenConfirmTitle");
    var dialogMessage = stringBundle.formatStringFromName("addMicsumGenConfirmText", [generatorURL], 1);
      
    if (!this.promptService.confirm(null, titleMessage, dialogMessage))
        return;

    var ioService = Components.classes["@mozilla.org/network/io-service;1"].
                    getService(Components.interfaces.nsIIOService);
    var generatorURI = ioService.newURI(generatorURL, null, null);

    var microsummaryService = Components.classes["@mozilla.org/microsummary/service;1"].
                              getService(Components.interfaces.nsIMicrosummaryService);
    if (microsummaryService)
      microsummaryService.addGenerator(generatorURI);
}

// property of nsIClassInfo
nsSidebar.prototype.flags = nsIClassInfo.DOM_OBJECT;

// property of nsIClassInfo
nsSidebar.prototype.classDescription = "Sidebar";

// method of nsIClassInfo
nsSidebar.prototype.getInterfaces = function(count) {
    var interfaceList = [nsISidebar, nsISidebarExternal, nsIClassInfo];
    count.value = interfaceList.length;
    return interfaceList;
}

// method of nsIClassInfo
nsSidebar.prototype.getHelperForLanguage = function(count) {return null;}

nsSidebar.prototype.QueryInterface =
function (iid) {
    if (!iid.equals(nsISidebar) &&
				!iid.equals(nsISidebarExternal) &&
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
                            
    catman.addCategoryEntry(JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY,
                            "external",
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
