/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is The JavaScript Debugger.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
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

/* components defined in this file */
const CLINE_SERVICE_CTRID =
    "@mozilla.org/commandlinehandler/general-startup;1?type=venkman";
const CLINE_SERVICE_CID =
    Components.ID("{18269616-1dd2-11b2-afa8-b612439bda27}");
const JSDPROT_HANDLER_CTRID =
    "@mozilla.org/network/protocol;1?name=x-jsd";
const JSDPROT_HANDLER_CID =
    Components.ID("{12ec790d-304e-4525-89a9-3e723d489d14}");
const JSDCNT_HANDLER_CTRID =
    "@mozilla.org/uriloader/content-handler;1?type=x-application-jsd";
const JSDCNT_HANDLER_CID =
    Components.ID("{306670f0-47bb-466b-b53b-613235623bbd}");

/* components used by this file */
const CATMAN_CTRID = "@mozilla.org/categorymanager;1";
const STRING_STREAM_CTRID = "@mozilla.org/io/string-input-stream;1";
const MEDIATOR_CTRID =
    "@mozilla.org/appshell/window-mediator;1";
const ASS_CONTRACTID =
    "@mozilla.org/appshell/appShellService;1";
const SIMPLEURI_CTRID = "@mozilla.org/network/simple-uri;1";

const nsIWindowMediator    = Components.interfaces.nsIWindowMediator;
const nsIAppShellService   = Components.interfaces.nsIAppShellService;
const nsICmdLineHandler    = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager   = Components.interfaces.nsICategoryManager;
const nsIContentHandler    = Components.interfaces.nsIContentHandler;
const nsIProtocolHandler   = Components.interfaces.nsIProtocolHandler;
const nsIURI               = Components.interfaces.nsIURI;
const nsIURL               = Components.interfaces.nsIURL;
const nsIStringInputStream = Components.interfaces.nsIStringInputStream;
const nsIChannel           = Components.interfaces.nsIChannel;
const nsIRequest           = Components.interfaces.nsIRequest;
const nsIProgressEventSink = Components.interfaces.nsIProgressEventSink;
const nsISupports          = Components.interfaces.nsISupports;

function findDebuggerWindow ()
{
    var windowManager =
        Components.classes[MEDIATOR_CTRID].getService(nsIWindowMediator);

    var window = windowManager.getMostRecentWindow("mozapp:venkman");

    return window;
}

function safeHTML(str)
{
    function replaceChars(ch)
    {
        switch (ch)
        {
            case "<":
                return "&lt;";
            
            case ">":
                return "&gt;";
                    
            case "&":
                return "&amp;";
        }

        return "?";
    };
        
    return String(str).replace(/[<>&]/g, replaceChars);
}

/* Command Line handler service */
function CLineService()
{}

CLineService.prototype.commandLineArgument = "-venkman";
CLineService.prototype.prefNameForStartup = "general.startup.venkman";
CLineService.prototype.chromeUrlForTask = "chrome://venkman/content";
CLineService.prototype.helpText = "Start with JavaScript Debugger.";
CLineService.prototype.handlesArgs = false;
CLineService.prototype.defaultArgs = "";
CLineService.prototype.openWindowWithArgs = false;

/* factory for command line handler service (CLineService) */
var CLineFactory = new Object();

CLineFactory.createInstance =
function clf_create (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsICmdLineHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new CLineService();
}

/* x-jsd: protocol handler */

const JSD_DEFAULT_PORT = 2206; /* Dana's apartment number. */

/* protocol handler factory object */
var JSDProtocolHandlerFactory = new Object();

JSDProtocolHandlerFactory.createInstance =
function jsdhf_create (outer, iid) {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIProtocolHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new JSDProtocolHandler();
}

function JSDURI (spec, charset)
{
    this.spec = this.prePath = spec;
    this.charset = this.originCharset = charset;
}

JSDURI.prototype.QueryInterface =
function jsdch_qi (iid)
{
    if (!iid.equals(nsIURI) && !iid.equals(nsIURL) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

JSDURI.prototype.scheme = "x-jsd";

JSDURI.prototype.fileBaseName =
JSDURI.prototype.fileExtension =
JSDURI.prototype.filePath  =
JSDURI.prototype.param     =
JSDURI.prototype.query     =
JSDURI.prototype.ref       =
JSDURI.prototype.directory =
JSDURI.prototype.fileName  =
JSDURI.prototype.username  =
JSDURI.prototype.password  =
JSDURI.prototype.hostPort  =
JSDURI.prototype.path      =
JSDURI.prototype.asciiHost =
JSDURI.prototype.userPass  = "";

JSDURI.prototype.port = JSD_DEFAULT_PORT;

JSDURI.prototype.schemeIs =
function jsduri_schemeis (scheme)
{
    return scheme.toLowerCase() == "x-jsd";
}

JSDURI.prototype.getCommonBaseSpec =
function jsduri_commonbase (uri)
{
    return "x-jsd:";
}

JSDURI.prototype.getRelativeSpec =
function jsduri_commonbase (uri)
{
    return uri;
}

JSDURI.prototype.equals =
function jsduri_equals (uri)
{
    return uri.spec == this.spec;
}

JSDURI.prototype.clone =
function jsduri_clone ()
{
    return new JSDURI (this.spec);
}

JSDURI.prototype.resolve =
function jsduri_resolve(path)
{
    //dump ("resolve " + path + " from " + this.spec + "\n");
    if (path[0] == "#")
        return this.spec + path;
    
    return path;
}

function JSDProtocolHandler()
{
    /* nothing here */
}

JSDProtocolHandler.prototype.scheme = "x-jsd";
JSDProtocolHandler.prototype.defaultPort = JSD_DEFAULT_PORT;
JSDProtocolHandler.prototype.protocolFlags = nsIProtocolHandler.URI_NORELATIVE ||
                                             nsIProtocolHandler.URI_NOAUTH;

JSDProtocolHandler.prototype.allowPort =
function jsdph_allowport (aPort, aScheme)
{
    return false;
}

JSDProtocolHandler.prototype.newURI =
function jsdph_newuri (spec, charset, baseURI)
{
    var clazz = Components.classes[SIMPLEURI_CTRID];
    var uri = clazz.createInstance(nsIURI);
    uri.spec = spec;
    return uri;
}

JSDProtocolHandler.prototype.newChannel =
function jsdph_newchannel (uri)
{
    return new JSDChannel (uri);
}

function JSDChannel (uri)
{
    this.URI = uri;
    this.originalURI = uri;
    this._isPending = true;
    var clazz = Components.classes[STRING_STREAM_CTRID];
    this.stringStream = clazz.createInstance(nsIStringInputStream);
}

JSDChannel.prototype.QueryInterface =
function jsdch_qi (iid)
{

    if (!iid.equals(nsIChannel) && !iid.equals(nsIRequest) &&
        !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

/* nsIChannel */
JSDChannel.prototype.loadAttributes = null;
JSDChannel.prototype.contentType = "text/html";
JSDChannel.prototype.contentLength = -1;
JSDChannel.prototype.owner = null;
JSDChannel.prototype.loadGroup = null;
JSDChannel.prototype.notificationCallbacks = null;
JSDChannel.prototype.securityInfo = null;

JSDChannel.prototype.open =
function jsdch_open()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

JSDChannel.prototype.asyncOpen =
function jsdch_aopen (streamListener, context)
{
    this.streamListener = streamListener;
    this.context = context;
    
    if (!window && this.URI.spec == "x-jsd:debugger")
    {
        this.contentType = "x-application-jsd";
        this.contentLength = 0;
        streamListener.onStartRequest(this, context);
        return;
    }
    
    var window = findDebuggerWindow();
    var ary = this.URI.spec.match (/x-jsd:([^:]+)/);
    var exception;

    if (this.loadGroup)
        this.loadGroup.addRequest (this, null);

    if (window && "console" in window && ary)
    {
        try
        {
            window.asyncOpenJSDURL (this, streamListener, context);
            return;
        }
        catch (ex)
        {
            exception = ex;
        }
    }

    var str =
        "<html><head><title>Error</title></head><body>Could not load &lt;<b>" +
        safeHTML(this.URI.spec) + "</b>&gt;<br>";
    
    if (!ary)
    {
        str += "<b>Error parsing uri.</b>";
    }
    else if (exception)
    {
        str += "<b>Internal error: " + safeHTML(exception) + "</b><br><pre>" + 
            safeHTML(exception.stack);
    }
    else
    {
        str += "<b>Debugger is not running.</b>";
    }
    
    str += "</body></html>";
    
    this.respond (str);
}

JSDChannel.prototype.respond =
function jsdch_respond (str)
{
    this.streamListener.onStartRequest (this, this.context);

    var len = str.length;
    this.stringStream.setData (str, len);
    this.streamListener.onDataAvailable (this, this.context,
                                         this.stringStream, 0, len);
    this.streamListener.onStopRequest (this, this.context,
                                       Components.results.NS_OK);
    if (this.loadGroup)
        this.loadGroup.removeRequest (this, null, Components.results.NS_OK);
    this._isPending = false;    
}

/* nsIRequest */
JSDChannel.prototype.isPending =
function jsdch_ispending ()
{
    return this._isPending;
}

JSDChannel.prototype.status = Components.results.NS_OK;

JSDChannel.prototype.cancel =
function jsdch_cancel (status)
{
    if (this._isPending)
    {
        this._isPending = false;
        this.streamListener.onStopRequest (this, this.context, status);
        if (this.loadGroup)
        {
            try
            {
                this.loadGroup.removeRequest (this, null, status);
            }
            catch (ex)
            {
                debug ("we're not in the load group?\n");
            }
        }
    }
    
    this.status = status;
}

JSDChannel.prototype.suspend =
JSDChannel.prototype.resume =
function jsdch_notimpl ()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

/*****************************************************************************/

/* x-application-jsd content handler */
function JSDContentHandler ()
{}

JSDContentHandler.prototype.QueryInterface =
function jsdh_qi(iid)
{
    if (!iid.equals(nsIContentHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    return this;
}

JSDContentHandler.prototype.handleContent =
function jsdh_handle(contentType, windowTarget, request)
{
    var e;
    var channel = request.QueryInterface(nsIChannel);
    
    // prevent someone from invoking the debugger remotely by serving
    // up any old file with the x-application-jsd content type.
    if (channel.URI.spec != "x-jsd:debugger")
    {
        debug ("Not handling content from unknown location ``" +
               channel.URI.spec + "''");
        return;
    }
    
    var window = findDebuggerWindow()

    if (window)
    {
        window.focus();
    }
    else
    {
        var ass =
            Components.classes[ASS_CONTRACTID].getService(nsIAppShellService);
        window = ass.hiddenDOMWindow;

        var args = new Object();
        args.url = channel.URI.spec;

        window.openDialog("chrome://venkman/content/venkman.xul", "_blank",
                          "chrome,menubar,toolbar,resizable,dialog=no", args);
    }
}

/*****************************************************************************/

/* content handler factory object (IRCContentHandler) */
var JSDContentHandlerFactory = new Object();

JSDContentHandlerFactory.createInstance =
function jsdhf_create(outer, iid)
{
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(nsIContentHandler) && !iid.equals(nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return new JSDContentHandler();
}

/*****************************************************************************/

var Module = new Object();

Module.registerSelf =
function (compMgr, fileSpec, location, type)
{
    debug("*** Registering -venkman handler.\n");
    
    compMgr =
        compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(CLINE_SERVICE_CID,
                                    "Venkman CommandLine Service",
                                    CLINE_SERVICE_CTRID, 
                                    fileSpec,
                                    location, 
                                    type);

	catman = Components.classes[CATMAN_CTRID].getService(nsICategoryManager);
	catman.addCategoryEntry("command-line-argument-handlers",
                            "venkman command line handler",
                            CLINE_SERVICE_CTRID, true, true);

    debug("*** Registering x-jsd protocol handler.\n");
    compMgr.registerFactoryLocation(JSDPROT_HANDLER_CID,
                                    "x-jsd protocol handler",
                                    JSDPROT_HANDLER_CTRID, 
                                    fileSpec, 
                                    location,
                                    type);

    debug("*** Registering x-application-jsd content handler.\n");
    compMgr.registerFactoryLocation(JSDCNT_HANDLER_CID,
                                    "x-application-jsd content handler",
                                    JSDCNT_HANDLER_CTRID, 
                                    fileSpec, 
                                    location,
                                    type);
    try
    {
        const JSD_CTRID = "@mozilla.org/js/jsd/debugger-service;1";
        const jsdIDebuggerService = Components.interfaces.jsdIDebuggerService;
        var jsds = Components.classes[JSD_CTRID].getService(jsdIDebuggerService);
        jsds.initAtStartup = true;
    }
    catch (ex)
    {
        debug ("*** ERROR initializing debugger service");
        debug (ex);
    }
}

Module.unregisterSelf =
function(compMgr, fileSpec, location)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(CLINE_SERVICE_CID, fileSpec);
	catman = Components.classes[CATMAN_CTRID].getService(nsICategoryManager);
	catman.deleteCategoryEntry("command-line-argument-handlers",
                               CLINE_SERVICE_CTRID, true);
}

Module.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(CLINE_SERVICE_CID))
        return CLineFactory;

    if (cid.equals(JSDPROT_HANDLER_CID))
        return JSDProtocolHandlerFactory;

    if (cid.equals(JSDCNT_HANDLER_CID))
        return JSDContentHandlerFactory;
    
    if (!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    throw Components.results.NS_ERROR_NO_INTERFACE;
    
}

Module.canUnload =
function(compMgr)
{
    return true;
}

/* entrypoint */
function NSGetModule(compMgr, fileSpec) {
    return Module;
}
