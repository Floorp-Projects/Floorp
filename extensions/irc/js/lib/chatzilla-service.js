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
 *   Robert Ginda <rginda@netscape.com>
 *   Justin Arthur <justinarthur@ieee.org>
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
 * This file contains the following chatzilla related components:
 * 1. Command line handler service, for responding to the -chat command line
 *    option. (CLineHandler)
 * 2. Content handlers for responding to content of type x-application-irc[s]
 *    (IRCContentHandler)
 * 3. Protocol handler for supplying a channel to the browser when an irc://
 *    or ircs:// link is clicked. (IRCProtocolHandler)
 * 4. A (nearly empty) implementation of nsIChannel for telling the browser
 *    that irc:// links have the content type x-application-irc[s] (BogusChannel)
 */

/* components defined in this file */
const CLINE_SERVICE_CONTRACTID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=chat";
const CLINE_SERVICE_CID =
    Components.ID("{38a95514-1dd2-11b2-97e7-9da958640f2c}");
//const IRCCONTENT_HANDLER_CONTRACTID =
//    "@mozilla.org/uriloader/content-handler;1?type=x-application-irc";
//const IRCCONTENT_HANDLER_CID =
//    Components.ID("{98919a14-1dd1-11b2-be1a-b84344307f0a}");
const IRCCONTENT_LISTENER_CONTRACTID =
    "@mozilla.org/uriloader/irc-external-content-listener;1";
const IRCSCONTENT_LISTENER_CONTRACTID =
    "@mozilla.org/uriloader/ircs-external-content-listener;1";
const IRCCONTENT_LISTENER_CID =
    Components.ID("{0e339944-6f43-47e2-a6b5-989b1b5dd84c}");
//    Components.ID("{98919a14-1dd1-11b2-be1a-b84344307f0a}");
const IRCSCONTENT_LISTENER_CID =
    Components.ID("{0e339944-6f43-47e2-a6b5-989b1b5dd84d}");
const IRCPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=irc";
const IRCSPROT_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=ircs";
const IRCPROT_HANDLER_CID =
    Components.ID("{f21c35f4-1dd1-11b2-a503-9bf8a539ea39}");
const IRCSPROT_HANDLER_CID =
    Components.ID("{f21c35f4-1dd1-11b2-a503-9bf8a539ea3a}");

const IRC_MIMETYPE = "application/x-irc";
const IRCS_MIMETYPE = "application/x-ircs";


/* components used in this file */
const MEDIATOR_CONTRACTID =
    "@mozilla.org/appshell/window-mediator;1";
const STANDARDURL_CONTRACTID = 
    "@mozilla.org/network/standard-url;1";
const IOSERVICE_CONTRACTID = 
    "@mozilla.org/network/io-service;1";
const ASS_CONTRACTID =
    "@mozilla.org/appshell/appShellService;1";
const RDFS_CONTRACTID =
    "@mozilla.org/rdf/rdf-service;1";

/* interafces used in this file */
const nsIWindowMediator  = Components.interfaces.nsIWindowMediator;
const nsICmdLineHandler  = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsIContentHandler  = Components.interfaces.nsIContentHandler;
const nsIURIContentListener = Components.interfaces.nsIURIContentListener;
const nsIURILoader       = Components.interfaces.nsIURILoader;
const nsIProtocolHandler = Components.interfaces.nsIProtocolHandler;
const nsIURI             = Components.interfaces.nsIURI;
const nsIStandardURL     = Components.interfaces.nsIStandardURL;
const nsIChannel         = Components.interfaces.nsIChannel;
const nsIRequest         = Components.interfaces.nsIRequest;
const nsIIOService       = Components.interfaces.nsIIOService;
const nsIAppShellService = Components.interfaces.nsIAppShellService;
const nsISupports        = Components.interfaces.nsISupports;
const nsISupportsWeakReference = Components.interfaces.nsISupportsWeakReference;
const nsIRDFService      = Components.interfaces.nsIRDFService;

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

/* content listener */
function IRCContentListener(isSecure)
{
    this.isSecure = isSecure;
}

IRCContentListener.prototype.QueryInterface =
function irccl_QueryInterface(iid)
{
    if (!iid.equals(nsIURIContentListener) && 
        !iid.equals(nsISupportsWeakReference) &&
        !iid.equals(nsISupports))
    {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
}

IRCContentListener.prototype.loadCookie = null;
IRCContentListener.prototype.parentContentListener = null;

IRCContentListener.prototype.onStartURIOpen =
function irccl_onStartURIOpen(uri)
{
}

IRCContentListener.prototype.doContent =
function irccl_doContent(contentType, preferred, request, contentHandler, count)
{
    var e;
    var channel = request.QueryInterface(nsIChannel);
    
    var wmClass = Components.classes[MEDIATOR_CONTRACTID];
    var windowManager = wmClass.getService(nsIWindowMediator);
    
    var assClass = Components.classes[ASS_CONTRACTID];
    var ass = assClass.getService(nsIAppShellService);
    hiddenWin = ass.hiddenDOMWindow;
    
    // Ok, not starting currently, so check if we've got existing windows.
    var w = windowManager.getMostRecentWindow("irc:chatzilla");
    
    // Claiming that a ChatZilla window is loading.
    if ("ChatZillaStarting" in hiddenWin)
    {
        dump("cz-service: ChatZilla claiming to be starting.\n");
        if (w && ("client" in w) && ("initialized" in w.client) && 
            w.client.initialized)
        {
            dump("cz-service: It lied. It's finished starting.\n");
            // It's actually loaded ok.
            delete hiddenWin.ChatZillaStarting;
        }
    }
    
    if ("ChatZillaStarting" in hiddenWin)
    {
        count = count || 0;
        
        if ((new Date() - hiddenWin.ChatZillaStarting) > 10000)
        {
            dump("cz-service: Continuing to be unable to talk to existing window!\n");
        }
        else
        {
            //dump("cz-service: **** Try: " + count + ", delay: " + (new Date() - hiddenWin.ChatZillaStarting) + "\n");
            
            // We have a ChatZilla window, but we're still loading.
            hiddenWin.setTimeout(function wrapper(t, count) {
                    t.doContent(contentType, preferred, request, contentHandler, count + 1);
                }, 250, this, count);
            return true;
        }
    }
    
    // We have a window.
    if (w)
    {
        dump("cz-service: Existing, fully loaded window. Using.\n");
        // Window is working and initialized ok. Use it.
        w.focus();
        w.gotoIRCURL(channel.URI.spec);
        return true;
    }
    
    dump("cz-service: No windows, starting new one.\n");
    // Ok, no available window, loading or otherwise, so start ChatZilla.
    var args = new Object();
    args.url = channel.URI.spec;
    
    hiddenWin.ChatZillaStarting = new Date();
    hiddenWin.openDialog("chrome://chatzilla/content/chatzilla.xul", "_blank",
                 "chrome,menubar,toolbar,status,resizable,dialog=no",
                 args);
    
    return true;
}

IRCContentListener.prototype.isPreferred =
function irccl_isPreferred(contentType, desiredContentType)
{
    return (contentType == (this.isSecure ? IRCS_MIMETYPE : IRC_MIMETYPE));
}

IRCContentListener.prototype.canHandleContent =
function irccl_canHandleContent(contentType, isContentPreferred, desiredContentType)
{
    return (contentType == (this.isSecure ? IRCS_MIMETYPE : IRC_MIMETYPE));
}

/* protocol handler factory object (IRCContentListener) */
var IRCContentListenerFactory = new Object();

IRCContentListenerFactory.createInstance =
function ircclf_createInstance(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
    
    if (!iid.equals(nsIURIContentListener) && 
        !iid.equals(nsISupportsWeakReference) &&
        !iid.equals(nsISupports))
    {
        throw Components.results.NS_ERROR_INVALID_ARG;
    }
    
    return new IRCContentListener(false);
}

/* secure protocol handler factory object (IRCContentListener) */
var IRCSContentListenerFactory = new Object();

IRCSContentListenerFactory.createInstance =
function ircclf_createInstance(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
    
    if (!iid.equals(nsIURIContentListener) && 
        !iid.equals(nsISupportsWeakReference) &&
        !iid.equals(nsISupports))
    {
        throw Components.results.NS_ERROR_INVALID_ARG;
    }
    
    return new IRCContentListener(true);
}

/* irc protocol handler component */
function IRCProtocolHandler(isSecure)
{
    this.isSecure = isSecure;
}

IRCProtocolHandler.prototype.protocolFlags = 
                   nsIProtocolHandler.URI_NORELATIVE |
                   nsIProtocolHandler.ALLOWS_PROXY;

IRCProtocolHandler.prototype.allowPort =
function ircph_allowPort(port, scheme)
{
    return false;
}

IRCProtocolHandler.prototype.newURI =
function ircph_newURI(spec, charset, baseURI)
{
    var cls = Components.classes[STANDARDURL_CONTRACTID];
    var url = cls.createInstance(nsIStandardURL);
    url.init(nsIStandardURL.URLTYPE_STANDARD, (this.isSecure ? 9999 : 6667), spec, charset, baseURI);

    return url.QueryInterface(nsIURI);
}

IRCProtocolHandler.prototype.newChannel =
function ircph_newChannel(URI)
{
    ios = Components.classes[IOSERVICE_CONTRACTID].getService(nsIIOService);
    if (!ios.allowPort(URI.port, URI.scheme))
        throw Components.results.NS_ERROR_FAILURE;
    
    var bogusChan = new BogusChannel(URI, this.isSecure);
    bogusChan.contentType = (this.isSecure ? IRCS_MIMETYPE : IRC_MIMETYPE);
    return bogusChan;
}

/* protocol handler factory object (IRCProtocolHandler) */
var IRCProtocolHandlerFactory = new Object();

IRCProtocolHandlerFactory.createInstance =
function ircphf_createInstance(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
    
    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;
    
    var protHandler = new IRCProtocolHandler(false);
    protHandler.scheme = "irc";
    protHandler.defaultPort = 6667;
    return protHandler;
}

/* secure protocol handler factory object (IRCProtocolHandler) */
var IRCSProtocolHandlerFactory = new Object();

IRCSProtocolHandlerFactory.createInstance =
function ircphf_createInstance(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
    
    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;
    
    var protHandler = new IRCProtocolHandler(true);
    protHandler.scheme = "ircs";
    protHandler.defaultPort = 9999;
    return protHandler;
}

/* bogus IRC channel used by the IRCProtocolHandler */
function BogusChannel(URI, isSecure)
{
    this.URI = URI;
    this.originalURI = URI;
    this.isSecure = isSecure;
}

BogusChannel.prototype.QueryInterface =
function bc_QueryInterface(iid)
{
    if (!iid.equals(nsIChannel) && !iid.equals(nsIRequest) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    
    return this;
}

/* nsIChannel */
BogusChannel.prototype.loadAttributes = null;
BogusChannel.prototype.contentLength = 0;
BogusChannel.prototype.owner = null;
BogusChannel.prototype.loadGroup = null;
BogusChannel.prototype.notificationCallbacks = null;
BogusChannel.prototype.securityInfo = null;

BogusChannel.prototype.open =
function bc_open()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

BogusChannel.prototype.asyncOpen =
function bc_asyncOpen(observer, ctxt)
{
    observer.onStartRequest(this, ctxt);
}

BogusChannel.prototype.asyncRead =
function bc_asyncRead(listener, ctxt)
{
    return listener.onStartRequest(this, ctxt);
}

/* nsIRequest */
BogusChannel.prototype.isPending =
function bc_isPending()
{
    return true;
}

BogusChannel.prototype.status = Components.results.NS_OK;

BogusChannel.prototype.cancel =
function bc_cancel(status)
{
    this.status = status;
}

BogusChannel.prototype.suspend =
BogusChannel.prototype.resume =
function bc_suspres()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

var ChatzillaModule = new Object();

ChatzillaModule.registerSelf =
function cz_mod_registerSelf(compMgr, fileSpec, location, type)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);
    
    debug("*** Registering -chat handler.\n");
    compMgr.registerFactoryLocation(CLINE_SERVICE_CID,
                                    "Chatzilla CommandLine Service",
                                    CLINE_SERVICE_CONTRACTID, 
                                    fileSpec, location, type);
    catman.addCategoryEntry("command-line-argument-handlers",
                            "chatzilla command line handler",
                            CLINE_SERVICE_CONTRACTID, true, true);
    
    debug("*** Registering content listener.\n");
    compMgr.registerFactoryLocation(IRCCONTENT_LISTENER_CID,
                                    "IRC content listener",
                                    IRCCONTENT_LISTENER_CONTRACTID, 
                                    fileSpec, location, type);
    catman.addCategoryEntry("external-uricontentlisteners",
                            IRC_MIMETYPE,
                            IRCCONTENT_LISTENER_CONTRACTID, true, true);
    
    debug("*** Registering secure content listener.\n");
    compMgr.registerFactoryLocation(IRCSCONTENT_LISTENER_CID,
                                    "IRC content listener",
                                    IRCSCONTENT_LISTENER_CONTRACTID, 
                                    fileSpec, location, type);
    catman.addCategoryEntry("external-uricontentlisteners",
                            IRCS_MIMETYPE,
                            IRCSCONTENT_LISTENER_CONTRACTID, true, true);
    
    debug("*** Registering irc protocol handler.\n");
    compMgr.registerFactoryLocation(IRCPROT_HANDLER_CID,
                                    "IRC protocol handler",
                                    IRCPROT_HANDLER_CONTRACTID, 
                                    fileSpec, location, type);
                                    
    debug("*** Registering ircs protocol handler.\n");
    compMgr.registerFactoryLocation(IRCSPROT_HANDLER_CID,
                                    "IRCS protocol handler",
                                    IRCSPROT_HANDLER_CONTRACTID, 
                                    fileSpec, location, type);
    
    debug("*** Registering done.\n");
}

ChatzillaModule.unregisterSelf =
function cz_mod_unregisterSelf(compMgr, fileSpec, location)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
        .getService(nsICategoryManager);
    catman.deleteCategoryEntry("command-line-argument-handlers",
                               CLINE_SERVICE_CONTRACTID, true);
}

ChatzillaModule.getClassObject =
function cz_mod_getClassObject(compMgr, cid, iid)
{
    // Checking if we're disabled in the Chrome Registry.
    var rv;
    try {
        var rdfSvc = Components.classes[RDFS_CONTRACTID].getService(nsIRDFService);
        var rdfDS = rdfSvc.GetDataSource("rdf:chrome");
        var resSelf = rdfSvc.GetResource("urn:mozilla:package:chatzilla");
        var resDisabled = rdfSvc.GetResource("http://www.mozilla.org/rdf/chrome#disabled");
        rv = rdfDS.GetTarget(resSelf, resDisabled, true);
    } catch (e) {
    }
    if (rv)
        throw Components.results.NS_ERROR_NO_INTERFACE;
    
    if (cid.equals(CLINE_SERVICE_CID))
        return CLineFactory;
    
    if (cid.equals(IRCCONTENT_LISTENER_CID))
        return IRCContentListenerFactory;
    
    if (cid.equals(IRCSCONTENT_LISTENER_CID))
        return IRCSContentListenerFactory;
    
    if (cid.equals(IRCPROT_HANDLER_CID))
        return IRCProtocolHandlerFactory;
    
    if (cid.equals(IRCSPROT_HANDLER_CID))
        return IRCSProtocolHandlerFactory;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    
    throw Components.results.NS_ERROR_NO_INTERFACE;
}

ChatzillaModule.canUnload =
function cz_mod_canUnload(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec)
{
    return ChatzillaModule;
}
