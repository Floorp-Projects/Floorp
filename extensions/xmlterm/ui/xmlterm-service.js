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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Robert Ginda <rginda@netscape.com>
 *   R. Saravanan <svn@xmlterm.org>
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
 * This file contains the following xmlterm related components:
 * 1. Command line handler service, for responding to the -terminal command line
 *    option. (XMLTermCLineHandler)
 * 2. Content handler for responding to content of type x-application-terminal
 *    (XMLTermContentHandler)
 * 3. Protocol handler for supplying a channel to the browser when an terminal://
 *    link is clicked. (XMLTermProtocolHandler)
 * 4. A (nearly empty) imeplementation of nsIChannel for telling the browser
 *    that terminal:// links have the content type x-application-terminal (BogusChannel)
 */

/* components defined in this file */
const XMLTERMCLINE_SERVICE_CONTRACTID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=terminal";
const XMLTERMCLINE_SERVICE_CID =
    Components.ID("{0eb82bE0-43a2-11d3-8e76-006008948af5}");
const XMLTERMCNT_HANDLER_CONTRACTID =
    "@mozilla.org/uriloader/content-handler;1?type=x-application-terminal";
const XMLTERMCNT_HANDLER_CID =
    Components.ID("{0eb82bE1-43a2-11d3-8e76-006008948af5}");
const XMLTERMPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=terminal";
const XMLTERMPROT_HANDLER_CID =
    Components.ID("{0eb82bE2-43a2-11d3-8e76-006008948af5}");

/* components used in this file */
const MEDIATOR_CONTRACTID =
    "@mozilla.org/appshell/window-mediator;1"
const SIMPLEURI_CONTRACTID =
    "@mozilla.org/network/simple-uri;1";
const ASS_CONTRACTID =
    "@mozilla.org/appshell/appShellService;1";
const SCRIPTSECURITYMANAGER_CONTRACTID =
    "@mozilla.org/scriptsecuritymanager;1";
const NS_IOSERVICE_CID_STR =
    "{9ac9e770-18bc-11d3-9337-00104ba0fd40}";

/* interfaces used in this file */
const nsIWindowMediator  = Components.interfaces.nsIWindowMediator;
const nsICmdLineHandler  = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsIContentHandler  = Components.interfaces.nsIContentHandler;
const nsIProtocolHandler = Components.interfaces.nsIProtocolHandler;
const nsIURI             = Components.interfaces.nsIURI;
const nsIChannel         = Components.interfaces.nsIChannel;
const nsIRequest         = Components.interfaces.nsIRequest;
const nsIAppShellService = Components.interfaces.nsIAppShellService;
const nsIIOService       = Components.interfaces.nsIIOService;
const nsIScriptSecurityManager = Components.interfaces.nsIScriptSecurityManager;
const nsISupports        = Components.interfaces.nsISupports;

/* Command Line handler service */
function XMLTermCLineService()
{}

XMLTermCLineService.prototype.commandLineArgument = "-terminal";
XMLTermCLineService.prototype.prefNameForStartup = "general.startup.terminal";
XMLTermCLineService.prototype.chromeUrlForTask="chrome://xmlterm/content";
XMLTermCLineService.prototype.helpText = "Start with a command line terminal";
XMLTermCLineService.prototype.handlesArgs=false;
XMLTermCLineService.prototype.defaultArgs ="";
XMLTermCLineService.prototype.openWindowWithArgs=false;

/* factory for command line handler service (XMLTermCLineService) */
XMLTermCLineFactory = new Object();

XMLTermCLineFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsICmdLineHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new XMLTermCLineService();
}

/* x-application-terminal content handler */
function XMLTermContentHandler ()
{}

XMLTermContentHandler.prototype.QueryInterface =
function (iid) {

    if (iid.equals(nsIContentHandler) ||
        iid.equals(nsISupports))
        return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
}

XMLTermContentHandler.prototype.handleContent =
function (aContentType, aWindowContext, aRequest)
{
    var e;

    var aChannel = aRequest.QueryInterface(Components.interfaces.nsIChannel);
		
    debug("XMLTermContentHandler.handleContent (" + aContentType + ", " +
          aWindowContext + ", " + aChannel.URI.spec + ")\n");

    var xmltermChromeURL = "chrome://xmlterm/content/xmlterm.xul?"+aChannel.URI.spec;
    //dump("XMLTermContentHandler:xmltermChromeURL = " + xmltermChromeURL + "\n");

    // Create new XMLterm window
    var appShellSvc = Components.classes[ASS_CONTRACTID].getService(nsIAppShellService);
    var domWin = appShellSvc.hiddenDOMWindow;
    domWin.open(xmltermChromeURL,"_blank", "chrome,menubar,toolbar,resizable");
}

/* content handler factory object (XMLTermContentHandler) */
XMLTermContentHandlerFactory = new Object();

XMLTermContentHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIContentHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new XMLTermContentHandler();
}

/* xmlterm protocol handler component */
function XMLTermProtocolHandler()
{
}

XMLTermProtocolHandler.prototype.scheme = "terminal";
XMLTermProtocolHandler.prototype.defaultPort = -1;
XMLTermProtocolHandler.prototype.URIType =
                 Components.interfaces.nsIProtocolHandler.URI_NORELATIVE;

XMLTermProtocolHandler.prototype.newURI =
function (aSpec, aCharset, aBaseURI)
{
    var uri = Components.classes[SIMPLEURI_CONTRACTID].createInstance(nsIURI);
    uri.spec = aSpec;

    return uri;
}

// "Global" variable
var gSystemPrincipal = null;

XMLTermProtocolHandler.prototype.newChannel =
function (aURI)
{
    var uriSpec = aURI.spec
    //dump("XMLTermProtocolHandler.newChannel: uriSpec="+uriSpec+"\n");

    if (uriSpec != "terminal:xmlterm")
       return new BogusChannel(aURI);

    // Re-direct to chrome HTML document, but with system principal

    var ioServ = Components.classesByID[NS_IOSERVICE_CID_STR].getService();
    ioServ = ioServ.QueryInterface(nsIIOService);

    // Open temporary XUL channel
    var xulURI = ioServ.newURI("chrome://xmlterm/content/xmltermDummy.xul",
                               null, null);
    var temChannel = ioServ.newChannelFromURI(xulURI);

    // Get owner of XUL channel
    var xulOwner = temChannel.owner;

    if (!gSystemPrincipal) {
       if (!xulOwner) {
          debug("XMLTermProtocolHandler: Internal error; unable to obtain system principal\n");
          throw Components.results.NS_ERROR_FAILURE;
       }
       gSystemPrincipal = xulOwner;
    }

    //dump("gSystemPrincipal="+gSystemPrincipal+"\n");

    // Cancel XUL request and release channel

	// why are you canceling here?! you have not even opened anything yet - dougt.
	// temChannel.cancel(Components.results.NS_BINDING_ABORTED);

	temChannel = null;

    // Get current process directory
    var dscontractid = "@mozilla.org/file/directory_service;1";
    var ds = Components.classes[dscontractid].getService();

    var dsprops = ds.QueryInterface(Components.interfaces.nsIProperties);
    var file = dsprops.get("CurProcD", Components.interfaces.nsIFile);

    file.append("chrome");
    file.append("xmlterm.jar");

    //dump("file="+file.path+"\n");

    // Contruct JAR URI spec for xmlterm.html
    // Use file: rather than resource: or chrome: scheme to allow
    // xmlterm to load other file URLs without failing the security check

    var jarURI = "jar:file:"+file.path+"!/content/xmlterm/xmlterm.html";

    var newChannel = ioServ.newChannel(jarURI, null, null);

    // Make new channel owned by system principal
    newChannel.owner = gSystemPrincipal;

    return newChannel;
}

/* protocol handler factory object (XMLTermProtocolHandler) */
XMLTermProtocolHandlerFactory = new Object();

XMLTermProtocolHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new XMLTermProtocolHandler();
}

/* bogus XMLTerm channel used by the XMLTermProtocolHandler */
function BogusChannel (aURI)
{
    this.URI = aURI;
    this.originalURI = aURI;
}

BogusChannel.prototype.QueryInterface =
function (iid) {

    if (iid.equals(nsIChannel) ||
        iid.equals(nsIRequest) ||
        iid.equals(nsISupports))
        return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
}

/* nsIChannel */
BogusChannel.prototype.loadAttributes = null;
BogusChannel.prototype.contentType = "x-application-terminal";
BogusChannel.prototype.contentLength = 0;
BogusChannel.prototype.owner = null;
BogusChannel.prototype.loadGroup = null;
BogusChannel.prototype.notificationCallbacks = null;
BogusChannel.prototype.securityInfo = null;
BogusChannel.prototype.shouldCache = false;

BogusChannel.prototype.open =
BogusChannel.prototype.asyncOpen =
function ()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

BogusChannel.prototype.asyncOpen =
function (observer, ctxt)
{
    observer.onStartRequest (this, ctxt);
}

/* nsIRequest */
BogusChannel.prototype.isPending =
function ()
{
    return true;
}

BogusChannel.prototype.status = Components.results.NS_OK;

BogusChannel.prototype.cancel =
function (aStatus)
{
    this.status = aStatus;
}

BogusChannel.prototype.parent =
BogusChannel.prototype.suspend =
BogusChannel.prototype.resume =
function ()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

var XMLtermModule = new Object();

XMLtermModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    debug("*** Registering -terminal handler.\n");

    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(XMLTERMCLINE_SERVICE_CID,
                                    "XMLterm CommandLine Service",
                                    XMLTERMCLINE_SERVICE_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    catman = Components.classes["@mozilla.org/categorymanager;1"]
               .getService(nsICategoryManager);
               	catman.addCategoryEntry("command-line-argument-handlers",
                             "terminal command line handler",
                 XMLTERMCLINE_SERVICE_CONTRACTID, true, true);

    debug("*** Registering x-application-terminal handler.\n");
    compMgr.registerFactoryLocation(XMLTERMCNT_HANDLER_CID,
                                    "XMLTerm Content Handler",
                                    XMLTERMCNT_HANDLER_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    debug("*** Registering terminal protocol handler.\n");
    compMgr.registerFactoryLocation(XMLTERMPROT_HANDLER_CID,
                                    "XMLTerm protocol handler",
                                    XMLTERMPROT_HANDLER_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

}

XMLtermModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    compMgr.unregisterFactoryLocation(XMLTERMCLINE_SERVICE_CID, fileSpec);
	catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);
	catman.deleteCategoryEntry("command-line-argument-handlers",
                                   XMLTERMCLINE_SERVICE_CONTRACTID, true);
}

XMLtermModule.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(XMLTERMCLINE_SERVICE_CID))
        return XMLTermCLineFactory;

    if (cid.equals(XMLTERMCNT_HANDLER_CID))
        return XMLTermContentHandlerFactory;

    if (cid.equals(XMLTERMPROT_HANDLER_CID))
        return XMLTermProtocolHandlerFactory;

    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    throw Components.results.NS_ERROR_NO_INTERFACE;

}

XMLtermModule.canUnload =
function(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return XMLtermModule;
}
