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
 * Robert Ginda <rginda@netscape.com>
 */

/*
 * This file contains the following chatzilla related components:
 * 1. Command line handler service, for responding to the -webcal command line
 *    option. (CLineHandler)
 * 2. Content handler for responding to content of type text/calendar
 *    (ICALContentHandler)
 * 3. Protocol handler for supplying a channel to the browser when an webcal://
 *    link is clicked. (ICALProtocolHandler)
 * 4. A (nearly empty) imeplementation of nsIChannel for telling the browser
 *    that webcal:// links have the content type text/calendar (BogusChannel)
 */

/* components defined in this file */
const CLINE_SERVICE_CONTRACTID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=webcal";
const CLINE_SERVICE_CID =
    Components.ID("{65ef4b0b-d116-4b93-bf8a-84525992bf27}");
const ICALCNT_HANDLER_CONTRACTID =
    "@mozilla.org/uriloader/content-handler;1?type=text/calendar";
const ICALCNT_HANDLER_CID =
    Components.ID("{9ebf4c8a-7770-40a6-aeed-e1738129535a}");
const ICALPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=webcal";
const ICALPROT_HANDLER_CID =
    Components.ID("{d320ba05-88cf-44a6-b718-87a72ef05918}");

/* components used in this file */
const MEDIATOR_CONTRACTID =
    "@mozilla.org/appshell/window-mediator;1";
const STANDARDURL_CONTRACTID =
    "@mozilla.org/network/standard-url;1";
const ASS_CONTRACTID =
    "@mozilla.org/appshell/appShellService;1";

/* interafces used in this file */
const nsIWindowMediator  = Components.interfaces.nsIWindowMediator;
const nsICmdLineHandler  = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsIContentHandler  = Components.interfaces.nsIContentHandler;
const nsIProtocolHandler = Components.interfaces.nsIProtocolHandler;
const nsIURI             = Components.interfaces.nsIURI;
const nsIStandardURL     = Components.interfaces.nsIStandardURL;
const nsIChannel         = Components.interfaces.nsIChannel;
const nsIRequest         = Components.interfaces.nsIRequest;
const nsIAppShellService = Components.interfaces.nsIAppShellService;
const nsISupports        = Components.interfaces.nsISupports;

/* Command Line handler service */
function CLineService()
{}

CLineService.prototype.commandLineArgument = "-webcal";
CLineService.prototype.prefNameForStartup = "general.startup.webal";
CLineService.prototype.chromeUrlForTask="chrome://calendar/content";
CLineService.prototype.helpText = "Start with a Calendar client";
CLineService.prototype.handlesArgs=false;
CLineService.prototype.defaultArgs ="";
CLineService.prototype.openWindowWithArgs=false;

/* factory for command line handler service (CLineService) */
var CLineFactory = new Object();

CLineFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsICmdLineHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new CLineService();
}

/* text/calendar content handler */
function ICALContentHandler ()
{}

ICALContentHandler.prototype.QueryInterface =
function (iid) {

    if (iid.equals(nsIContentHandler) ||
        iid.equals(nsISupports))
        return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
}

ICALContentHandler.prototype.handleContent =
function (aContentType, aWindowTarget, aRequest)
{
    var e;
    var channel = aRequest.QueryInterface(nsIChannel);

    /*
    dump ("ICALContentHandler.handleContent (" + aContentType + ", " +
           aWindowTarget + ", " + channel.URI.spec + ")\n");
    */

    var windowManager =
        Components.classes[MEDIATOR_CONTRACTID].getService(nsIWindowMediator);

    var w = windowManager.getMostRecentWindow("calendarMainWindow");

    if (w)
    {
        w.focus();

        w.gCalendarWindow.calendarManager.checkCalendarURL( channel );
    }
    else
    {
        var ass =
            Components.classes[ASS_CONTRACTID].getService(nsIAppShellService);
        w = ass.hiddenDOMWindow;

        var args = new Object ();
        args.channel = channel;
        w.openDialog("chrome://calendar/content/calendar.xul", "calendar", "chrome,menubar,resizable,scrollbars,status,toolbar,dialog=no", args);
    }

}

/* content handler factory object (ICALContentHandler) */
var ICALContentHandlerFactory = new Object();

ICALContentHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIContentHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new ICALContentHandler();
}

/* webcal protocol handler component */
function ICALProtocolHandler()
{
}

ICALProtocolHandler.prototype.scheme = "webcal";
ICALProtocolHandler.prototype.defaultPort = 8080;
ICALProtocolHandler.prototype.protocolFlags =
                   nsIProtocolHandler.URI_NORELATIVE |
                   nsIProtocolHandler.ALLOWS_PROXY;

ICALProtocolHandler.prototype.allowPort =
function (aPort, aScheme)
{
    return false;
}

ICALProtocolHandler.prototype.newURI =
function (aSpec, aCharset, aBaseURI)
{
    var url = Components.classes[STANDARDURL_CONTRACTID].
      createInstance(nsIStandardURL);
    url.init(nsIStandardURL.URLTYPE_STANDARD, 8080, aSpec, aCharset, aBaseURI);

    return url.QueryInterface(nsIURI);
}

ICALProtocolHandler.prototype.newChannel =
function (aURI)
{
    return new BogusChannel (aURI);
}

/* protocol handler factory object (ICALProtocolHandler) */
var ICALProtocolHandlerFactory = new Object();

ICALProtocolHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new ICALProtocolHandler();
}

/* bogus webcal channel used by the ICALProtocolHandler */
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
BogusChannel.prototype.contentType = "text/calendar";
BogusChannel.prototype.contentLength = 0;
BogusChannel.prototype.owner = null;
BogusChannel.prototype.loadGroup = null;
BogusChannel.prototype.notificationCallbacks = null;
BogusChannel.prototype.securityInfo = null;

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

var ChatzillaModule = new Object();

ChatzillaModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    dump("*** Registering -webcal handler.\n");

    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(CLINE_SERVICE_CID,
                                    "Calendar CommandLine Service",
                                    CLINE_SERVICE_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

	catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);
	catman.addCategoryEntry("command-line-argument-handlers",
                            "calendar command line handler",
                            CLINE_SERVICE_CONTRACTID, true, true);

    dump("*** Registering text/calendar handler.\n");
    compMgr.registerFactoryLocation(ICALCNT_HANDLER_CID,
                                    "Webcal Content Handler",
                                    ICALCNT_HANDLER_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    dump("*** Registering webcal protocol handler.\n");
    compMgr.registerFactoryLocation(ICALPROT_HANDLER_CID,
                                    "Webcal protocol handler",
                                    ICALPROT_HANDLER_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);
}

ChatzillaModule.unregisterSelf =
function(compMgr, fileSpec, location)
{

    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(CLINE_SERVICE_CID,
                                      fileSpec);
	catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);
	catman.deleteCategoryEntry("command-line-argument-handlers",
                               CLINE_SERVICE_CONTRACTID, true);
}

ChatzillaModule.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(CLINE_SERVICE_CID))
        return CLineFactory;

    if (cid.equals(ICALCNT_HANDLER_CID))
        return ICALContentHandlerFactory;

    if (cid.equals(ICALPROT_HANDLER_CID))
        return ICALProtocolHandlerFactory;

    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    throw Components.results.NS_ERROR_NO_INTERFACE;

}

ChatzillaModule.canUnload =
function(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return ChatzillaModule;
}
