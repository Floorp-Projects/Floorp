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
 * 1. Command line handler service, for responding to the -chat command line
 *    option. (CLineHandler)
 * 2. Content handler for responding to content of type x-application-irc
 *    (IRCContentHandler)
 * 3. Protocol handler for supplying a channel to the browser when an irc://
 *    link is clicked. (IRCProtocolHandler)
 * 4. A (nearly empty) imeplementation of nsIChannel for telling the browser
 *    that irc:// links have the content type x-application-irc (BogusChannel)
 */

/* components defined in this file */
const CLINE_SERVICE_CONTRACTID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=chat";
const CLINE_SERVICE_CID =
    Components.ID("{38a95514-1dd2-11b2-97e7-9da958640f2c}");
const IRCCNT_HANDLER_CONTRACTID =
    "@mozilla.org/uriloader/content-handler;1?type=x-application-irc";
const IRCCNT_HANDLER_CID =
    Components.ID("{98919a14-1dd1-11b2-be1a-b84344307f0a}");
const IRCPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=irc";
const IRCPROT_HANDLER_CID =
    Components.ID("{f21c35f4-1dd1-11b2-a503-9bf8a539ea39}");

/* components used in this file */
const MEDIATOR_CONTRACTID =
    "@mozilla.org/appshell/window-mediator;1";
const STANDARDURL_CONTRACTID = 
    "@mozilla.org/network/standard-url;1";
const IOSERVICE_CONTRACTID = 
    "@mozilla.org/network/io-service;1";
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
const nsIIOService       = Components.interfaces.nsIIOService;
const nsIAppShellService = Components.interfaces.nsIAppShellService;
const nsISupports        = Components.interfaces.nsISupports;

/* Command Line handler service */
function CLineService()
{}

CLineService.prototype.commandLineArgument = "-chat";
CLineService.prototype.prefNameForStartup = "general.startup.chat";
CLineService.prototype.chromeUrlForTask = "chrome://chatzilla/content";
CLineService.prototype.helpText = "Start with an IRC chat client";
CLineService.prototype.handlesArgs = false;
CLineService.prototype.defaultArgs = "";
CLineService.prototype.openWindowWithArgs = false;

/* factory for command line handler service (CLineService) */
var CLineFactory = new Object();

CLineFactory.createInstance =
function (outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsICmdLineHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new CLineService();
}

/* x-application-irc content handler */
function IRCContentHandler ()
{}

IRCContentHandler.prototype.QueryInterface =
function (iid)
{
    if (!iid.equals(nsIContentHandler))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

IRCContentHandler.prototype.handleContent =
function (contentType, command, windowTarget, request)
{
    var e;
    var channel = request.QueryInterface(nsIChannel);
    
    var windowManager =
        Components.classes[MEDIATOR_CONTRACTID].getService(nsIWindowMediator);

    var w = windowManager.getMostRecentWindow("irc:chatzilla");

    if (w)
    {
        w.focus();
        w.gotoIRCURL(channel.URI.spec);
    }
    else
    {
        var ass =
            Components.classes[ASS_CONTRACTID].getService(nsIAppShellService);
        w = ass.hiddenDOMWindow;

        var args = new Object ();
        args.url = channel.URI.spec;

        w.openDialog("chrome://chatzilla/content/chatzilla.xul", "_blank",
                     "chrome,menubar,toolbar,resizable,dialog=no", args);
    }
    
}

/* content handler factory object (IRCContentHandler) */
var IRCContentHandlerFactory = new Object();

IRCContentHandlerFactory.createInstance =
function (outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIContentHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new IRCContentHandler();
}

/* irc protocol handler component */
function IRCProtocolHandler()
{
}

IRCProtocolHandler.prototype.scheme = "irc";
IRCProtocolHandler.prototype.defaultPort = 6667;
IRCProtocolHandler.prototype.protocolFlags = 
                   nsIProtocolHandler.URI_NORELATIVE |
                   nsIProtocolHandler.ALLOWS_PROXY;

IRCProtocolHandler.prototype.allowPort =
function (port, scheme)
{
    return false;
}

IRCProtocolHandler.prototype.newURI =
function (spec, charset, baseURI)
{
    var cls = Components.classes[STANDARDURL_CONTRACTID];
    var url = cls.createInstance(nsIStandardURL);
    url.init(nsIStandardURL.URLTYPE_STANDARD, 6667, spec, charset, baseURI);

    return url.QueryInterface(nsIURI);
}

IRCProtocolHandler.prototype.newChannel =
function (URI)
{
    ios = Components.classes[IOSERVICE_CONTRACTID].getService(nsIIOService);
    if (!ios.allowPort(URI.port, URI.scheme))
        throw Components.results.NS_ERROR_FAILURE;

    return new BogusChannel (URI);
}

/* protocol handler factory object (IRCProtocolHandler) */
var IRCProtocolHandlerFactory = new Object();

IRCProtocolHandlerFactory.createInstance =
function (outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new IRCProtocolHandler();
}

/* bogus IRC channel used by the IRCProtocolHandler */
function BogusChannel (URI)
{
    this.URI = URI;
    this.originalURI = URI;
}

BogusChannel.prototype.QueryInterface =
function (iid)
{
    if (!iid.equals(nsIChannel) && !iid.equals(nsIRequest) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

/* nsIChannel */
BogusChannel.prototype.loadAttributes = null;
BogusChannel.prototype.contentType = "x-application-irc";
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
function (status)
{
    this.status = status;
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
    debug("*** Registering -chat handler.\n");
    
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(CLINE_SERVICE_CID,
                                    "Chatzilla CommandLine Service",
                                    CLINE_SERVICE_CONTRACTID, 
                                    fileSpec,
                                    location, 
                                    type);
    
	catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);
	catman.addCategoryEntry("command-line-argument-handlers",
                            "chatzilla command line handler",
                            CLINE_SERVICE_CONTRACTID, true, true);

    debug("*** Registering x-application-irc handler.\n");
    compMgr.registerFactoryLocation(IRCCNT_HANDLER_CID,
                                    "IRC Content Handler",
                                    IRCCNT_HANDLER_CONTRACTID, 
                                    fileSpec,
                                    location, 
                                    type);

    debug("*** Registering irc protocol handler.\n");
    compMgr.registerFactoryLocation(IRCPROT_HANDLER_CID,
                                    "IRC protocol handler",
                                    IRCPROT_HANDLER_CONTRACTID, 
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
function (compMgr, cid, iid)
{
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

ChatzillaModule.canUnload =
function(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec)
{
    return ChatzillaModule;
}
