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
 * Mike Potter <mikep@oeone.com>
 */

/*
 * This file contains the following chatzilla related components:
 * 1. Command line handler service, for responding to the -chat command line
 *    option. (CLineHandler)
 * 2. Content handler for responding to content of type x-application-irc
 *    (IRCContentHandler)
 * 3. Protocol handler for supplying a channel to the browser when an irc://
 *    link is clicked. (ICalProtocolHandler)
 * 4. A (nearly empty) imeplementation of nsIChannel for telling the browser
 *    that irc:// links have the content type x-application-irc (BogusChannel)
 */

/* components defined in this file */
const CLINE_SERVICE_CONTRACTID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=chat";
const CLINE_SERVICE_CID =
    Components.ID("{65ef4b0b-d116-4b93-bf8a-84525992bf27}");
const ICALCNT_HANDLER_CONTRACTID =
    "@mozilla.org/uriloader/content-handler;1?type=x-application-irc";
const ICALCNT_HANDLER_CID =
    Components.ID("{9ebf4c8a-7770-40a6-aeed-e1738129535a}");
const ICALPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=ical";
const IRCPROT_HANDLER_CID =
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

CLineService.prototype.commandLineArgument = "-calendar";
CLineService.prototype.prefNameForStartup = "general.startup.calendar";
CLineService.prototype.chromeUrlForTask="chrome://calendar/content";
CLineService.prototype.helpText = "Start with an Calendar client";
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

/* x-application-irc content handler */
function ICALContentHandler ()
{}

ICALContentHandler.prototype.QueryInterface =
function (iid) {

    if (!iid.equals(nsIContentHandler))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

ICALContentHandler.prototype.handleContent =
function (aContentType, aCommand, aWindowTarget, aRequest)
{
    var e;
    var channel = aRequest.QueryInterface(nsIChannel);
    
    /*
    debug ("ircLoader.handleContent (" + aContentType + ", " +
          aCommand + ", " + aWindowTarget + ", " + channel.URI.spec + ")\n");
    */

    var windowManager =
        Components.classes[MEDIATOR_CONTRACTID].getService(nsIWindowMediator);

    var w = windowManager.getMostRecentWindow("calendarMainWindow");

    if (w)
    {
        w.focus();
        w.focusEvent(channel.URI.spec);
    }
    else
    {
        var ass =
            Components.classes[ASS_CONTRACTID].getService(nsIAppShellService);
        w = ass.hiddenDOMWindow;

        var args = new Object ();
        args.url = channel.URI.spec;

        w.open("chrome://calendar/content/calendar.xul", "_blank",
                     "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar", args);
    }
    
}

/* content handler factory object (IRCContentHandler) */
var ICALContentHandlerFactory = new Object();

ICALContentHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIContentHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new ICALContentHandler();
}

/* irc protocol handler component */
function ICalProtocolHandler()
{
}

ICalProtocolHandler.prototype.scheme = "ical";
ICalProtocolHandler.prototype.protocolFlags = 
                   nsIProtocolHandler.URI_NORELATIVE |
                   nsIProtocolHandler.ALLOWS_PROXY;

ICalProtocolHandler.prototype.allowPort =
function (aPort, aScheme)
{
    return false;
}

ICalProtocolHandler.prototype.newURI =
function (aSpec, aCharset, aBaseURI)
{
    if (aBaseURI)
    {
        debug ("-*- ircHandler: aBaseURI passed to newURI, bailing.\n");
        return null;
    }
    
    var url = Components.classes[STANDARDURL_CONTRACTID].
      createInstance(nsIStandardURL);
    url.init(nsIStandardURL.URLTYPE_STANDARD, 0, aSpec, aCharset, aBaseURI);
    
    return url.QueryInterface(nsIURI);
}

/* protocol handler factory object (IRCProtocolHandler) */
var ICalProtocolHandlerFactory = new Object();

ICalProtocolHandlerFactory.createInstance =
function (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new ICalProtocolHandler();
}

var CalendarModule = new Object();

CalendarModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    debug("*** Registering -chat handler.\n");
    
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

    debug("*** Registering ical protocol handler.\n");
    compMgr.registerFactoryLocation(IRCPROT_HANDLER_CID,
                                    "ICAL protocol handler",
                                    IRCPROT_HANDLER_CONTRACTID, 
                                    fileSpec, 
                                    location,
                                    type);

}

CalendarModule.unregisterSelf =
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

CalendarModule.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(CLINE_SERVICE_CID))
        return CLineFactory;

    if (cid.equals(IRCCNT_HANDLER_CID))
        return IRCContentHandlerFactory;

    if (cid.equals(IRCPROT_HANDLER_CID))
        return IRCProtocolHandlerFactory;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    throw Components.results.NS_ERROR_NO_INTERFACE;
    
}

CalendarModule.canUnload =
function(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return CalendarModule;
}

