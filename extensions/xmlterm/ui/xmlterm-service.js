/*
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
 * Robert Ginda <rginda@netscape.com>
 * R. Saravanan <svn@xmlterm.org>
 */

/*
 * This file contains the following xmlterm related components:
 * 1. Command line handler service, for responding to the -terminal command line
 *    option. (CLineHandler)
 * 2. Content handler for responding to content of type x-application-telnet
 *    (TelnetContentHandler)
 * 3. Protocol handler for supplying a channel to the browser when an telnet://
 *    link is clicked. (TelnetProtocolHandler)
 * 4. A (nearly empty) imeplementation of nsIChannel for telling the browser
 *    that telnet:// links have the content type x-application-telnet (BogusChannel)
 */

/* components defined in this file */
const CLINE_SERVICE_CONTRACTID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=terminal";
const CLINE_SERVICE_CID =
    Components.ID("{0eb82bE0-43a2-11d3-8e76-006008948af5}");
const TELNETCNT_HANDLER_CONTRACTID =
    "@mozilla.org/uriloader/content-handler;1?type=x-application-telnet";
const TELNETCNT_HANDLER_CID =
    Components.ID("{0eb82bE1-43a2-11d3-8e76-006008948af5}");
const TELNETPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=telnet";
const TELNETPROT_HANDLER_CID =
    Components.ID("{0eb82bE2-43a2-11d3-8e76-006008948af5}");

/* components used in this file */
const MEDIATOR_CONTRACTID =
    "@mozilla.org/rdf/datasource;1?name=window-mediator"
const SIMPLEURI_CONTRACTID = 
    "@mozilla.org/network/simple-uri;1";
const ASS_CONTRACTID =
    "@mozilla.org/appshell/appShellService;1";

/* interafces used in this file */
const nsIWindowMediator  = Components.interfaces.nsIWindowMediator;
const nsICmdLineHandler  = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsIContentHandler  = Components.interfaces.nsIContentHandler;
const nsIProtocolHandler = Components.interfaces.nsIProtocolHandler;
const nsIURI             = Components.interfaces.nsIURI;
const nsIChannel         = Components.interfaces.nsIChannel;
const nsIRequest         = Components.interfaces.nsIRequest;
const nsIAppShellService = Components.interfaces.nsIAppShellService;
const nsISupports        = Components.interfaces.nsISupports;

/* Command Line handler service */
function CLineService()
{}

CLineService.prototype.commandLineArgument = "-terminal";
CLineService.prototype.prefNameForStartup = "general.startup.terminal";
CLineService.prototype.chromeUrlForTask="chrome://xmlterm/content";
CLineService.prototype.helpText = "Start with a command line terminal";
CLineService.prototype.handlesArgs=false;
CLineService.prototype.defaultArgs ="";
CLineService.prototype.openWindowWithArgs=false;

/* factory for command line handler service (CLineService) */
CLineFactory = new Object();

CLineFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsICmdLineHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new CLineService();
}

/* x-application-telnet content handler */
function TelnetContentHandler ()
{}

TelnetContentHandler.prototype.queryInterface =
function (iid) {

    if (!iid.equals(nsIContentHandler))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

TelnetContentHandler.prototype.handleContent =
function (aContentType, aCommand, aWindowTarget, aSourceContext, aChannel)
{
    var e;

    dump ("telnetLoader.handleContent (" + aContentType + ", " +
          aCommand + ", " + aWindowTarget + ", " + aSourceContext + ", " +
          aChannel.URI.spec + ")\n");

    var xmltermChromeURL = "chrome://xmlterm/content/xmlterm.xul?"+aChannel.URI.spec;
    dump("telnetLoader:xmltermChromeURL = " + xmltermChromeURL + "\n");

    var windowManager =
        Components.classes[MEDIATOR_CONTRACTID].getService(nsIWindowMediator);

    var w = windowManager.getMostRecentWindow("terminal:xmlterm");

//    Commented out because unlike chatzilla, xmlterm is not really a service
//    if (w)
//    {
//        // Shift focus to XMLterm window
//        w.focus();
//        w.gotoTelnetURL(aChannel.URI.spec);
//    } else

    // Create new XMLterm window
    var ass = Components.classes[ASS_CONTRACTID].getService(nsIAppShellService);
    var w = ass.getHiddenDOMWindow();
    w.open(xmltermChromeURL, "_blank", "chrome,menubar,toolbar,resizable");
    
}

/* content handler factory object (TelnetContentHandler) */
TelnetContentHandlerFactory = new Object();

TelnetContentHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIContentHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new TelnetContentHandler();
}

/* telnet protocol handler component */
function TelnetProtocolHandler()
{
}

TelnetProtocolHandler.prototype.scheme = "telnet";
TelnetProtocolHandler.prototype.defaultPort = 9999;

TelnetProtocolHandler.prototype.newURI =
function (aSpec, aBaseURI)
{
    if (aBaseURI)
    {
        dump ("-*- telnetHandler: aBaseURI passed to newURI, bailing.\n");
        return null;
    }
    
    var uri = Components.classes[SIMPLEURI_CONTRACTID].createInstance(nsIURI);
    uri.spec = aSpec;
    
    return uri;
}

TelnetProtocolHandler.prototype.newChannel =
function (aURI)
{
    return new BogusChannel (aURI);
}

/* protocol handler factory object (TelnetProtocolHandler) */
TelnetProtocolHandlerFactory = new Object();

TelnetProtocolHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new TelnetProtocolHandler();
}

/* bogus Telnet channel used by the TelnetProtocolHandler */
function BogusChannel (aURI)
{
    this.URI = aURI;
    this.originalURI = aURI;
}

BogusChannel.prototype.queryInterface =
function (iid) {

    if (!iid.equals(nsIChannel) && !iid.equals(nsIRequest) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

/* nsIChannel */
BogusChannel.prototype.transferOffset = 0;
BogusChannel.prototype.transferCount = 0;
BogusChannel.prototype.loadAttributes = null;
BogusChannel.prototype.contentType = "x-application-telnet";
BogusChannel.prototype.contentLength = 0;
BogusChannel.prototype.owner = null;
BogusChannel.prototype.loadGroup = null;
BogusChannel.prototype.notificationCallbacks = null;
BogusChannel.prototype.securityInfo = null;
BogusChannel.prototype.bufferSegmentSize = 0;
BogusChannel.prototype.bufferMaxSize = 0;
BogusChannel.prototype.shouldCache = false;
BogusChannel.prototype.pipeliningAllowed = false;

BogusChannel.prototype.openInputStream =
BogusChannel.prototype.openOutputStream =
BogusChannel.prototype.asyncWrite =
function ()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

BogusChannel.prototype.asyncOpen =
function (observer, ctxt)
{
    observer.onStartRequest (this, ctxt);
}

BogusChannel.prototype.asyncRead =
function (listener, ctxt)
{
    return listener.onStartRequest (this, ctxt);
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
    dump("*** Registering -terminal handler.\n");
    compMgr.registerComponentWithType(CLINE_SERVICE_CID,
                                      "XMLterm CommandLine Service",
                                      CLINE_SERVICE_CONTRACTID, fileSpec,
                                      location, true, true, type);
    
	catman = Components.classes["mozilla.categorymanager.1"]
        .getService(nsICategoryManager);
	catman.addCategoryEntry("command-line-argument-handlers",
                            "xmlterm command line handler",
                            CLINE_SERVICE_CONTRACTID, true, true);

    dump("*** Registering x-application-telnet handler.\n");
    compMgr.registerComponentWithType(TELNETCNT_HANDLER_CID,
                                      "Telnet Content Handler",
                                      TELNETCNT_HANDLER_CONTRACTID, fileSpec,
                                      location, true, true, type);

    dump("*** Registering telnet protocol handler.\n");
    compMgr.registerComponentWithType(TELNETPROT_HANDLER_CID,
                                      "Telnet protocol handler",
                                      TELNETPROT_HANDLER_CONTRACTID, fileSpec, location,
                                      true, true, type);

}

XMLtermModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
    compMgr.unregisterComponentSpec(CLINE_SERVICE_CID, fileSpec);
	catman = Components.classes["mozilla.categorymanager.1"]
        .getService(nsICategoryManager);
	catman.deleteCategoryEntry("command-line-argument-handlers",
                               CLINE_SERVICE_CONTRACTID, true);
}

XMLtermModule.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(CLINE_SERVICE_CID))
        return CLineFactory;

    if (cid.equals(TELNETCNT_HANDLER_CID))
        return TelnetContentHandlerFactory;

    if (cid.equals(TELNETPROT_HANDLER_CID))
        return TelnetProtocolHandlerFactory;
    
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
