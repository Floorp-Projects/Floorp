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
 * This file contains the following calendar related components:
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
    "@mozilla.org/commandlinehandler/general-startup;1?type=calendar";
const CLINE_SERVICE_CID =
    Components.ID("{65ef4b0b-d116-4b93-bf8a-84525992bf27}");
const ICALCNT_HANDLER_CONTRACTID =
    "@mozilla.org/uriloader/content-handler;1?type=text/calendar";
const ICALCNT_HANDLER_CID =
    Components.ID("{9ebf4c8a-7770-40a6-aeed-e1738129535a}");
const ICALPROT_HANDLER_CID =
    Components.ID("{d320ba05-88cf-44a6-b718-87a72ef05918}");

/* components used in this file */
const MEDIATOR_CONTRACTID =
    "@mozilla.org/appshell/window-mediator;1";
const STANDARDURL_CONTRACTID =
    "@mozilla.org/network/standard-url;1";
const ASS_CONTRACTID =
    "@mozilla.org/appshell/appShellService;1";
const WINDOWWATCHER_CONTRACTID =
    "@mozilla.org/embedcomp/window-watcher;1";

/* interafces used in this file */
const nsIWindowMediator  = Components.interfaces.nsIWindowMediator;
const nsICmdLineHandler  = Components.interfaces.nsICmdLineHandler;
const nsICategoryManager = Components.interfaces.nsICategoryManager;
const nsIContentHandler  = Components.interfaces.nsIContentHandler;
const nsIFactory         = Components.interfaces.nsIFactory;
const nsIProtocolHandler = Components.interfaces.nsIProtocolHandler;
const nsIURI             = Components.interfaces.nsIURI;
const nsIStandardURL     = Components.interfaces.nsIStandardURL;
const nsIChannel         = Components.interfaces.nsIChannel;
const nsIRequest         = Components.interfaces.nsIRequest;
const nsIAppShellService = Components.interfaces.nsIAppShellService;
const nsISupports        = Components.interfaces.nsISupports;
const nsIWindowWatcher   = Components.interfaces.nsIWindowWatcher;

if ("nsICommandLineHandler" in Components.interfaces)
     const nsICommandLineHandler = Components.interfaces.nsICommandLineHandler;

/* Command Line handler service */
function CLineService()
{}

CLineService.prototype = {
    /* nsISupports */
    QueryInterface : function service_qi(iid) {
        if (iid.equals(nsISupports))
            return this;

        if (nsICmdLineHandler && iid.equals(nsICmdLineHandler))
            return this;

        if (nsICommandLineHandler && iid.equals(nsICommandLineHandler))
            return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    /* nsICmdLineHandler */

    commandLineArgument : "-calendar",
    prefNameForStartup  : "general.startup.calendar",
    chromeUrlForTask    : "chrome://calendar/content",
    helpText            : "Start with calendar",
    handlesArgs         : false,
    defaultArgs         : "",
    openWindowWithArgs  : true,

    /* nsICommandLineHandler */

    handle : function service_handle(cmdLine) {
        // just pass all arguments on to the Sunbird window
        wwatch = Components.classes[WINDOWWATCHER_CONTRACTID]
                           .getService(nsIWindowWatcher);
        wwatch.openWindow(null, "chrome://calendar/content/",
                              "_blank", "chrome,dialog=no,all", cmdLine);
        cmdLine.preventDefault = true;
    },

    helpInfo : "  -subscribe or -url   Pass in a path pointing to a calendar\n" +
               "                       to subscribe to.\n" +
               "  -showdate            Pass in a value for a javascript date\n" +
               "                       to show this date on startup.\n"
};

/* factory for command line handler service (CLineService) */
var CLineFactory = {
    createInstance : function (outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        return new CLineService().QueryInterface(iid);
    }
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
function(aContentType, aWindowTarget, aRequest)
{
    var channel = aRequest.QueryInterface(nsIChannel);

    // Cancel the request ...
    var uri = channel.URI;
    const NS_BINDING_ABORTED = 0x804b0002 // from nsNetError.h
    aRequest.cancel(NS_BINDING_ABORTED);

    // ... Subscribe to the uri ...
    var calendarManager = Components.classes["@mozilla.org/calendar/manager;1"]
                                    .getService(Components.interfaces.calICalendarManager);

    var newCalendar = calendarManager.createCalendar('ics', uri);
    calendarManager.registerCalendar(newCalendar);

    // XXX Come up with a better name, like the filename or X-WR-CALNAME
    // XXX Also, make up a color
    newCalendar.name = "temp";

    // ... and open or focus a calendar window.
    var windowManager = Components.classes[MEDIATOR_CONTRACTID]
                                  .getService(nsIWindowMediator);

    var w = windowManager.getMostRecentWindow("calendarMainWindow");

    if (w) {
        w.focus();
    } else {
        var ass = Components.classes[ASS_CONTRACTID]
                            .getService(nsIAppShellService);
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

var CalendarModule = new Object();

CalendarModule.registerSelf =
function (compMgr, fileSpec, location, type)
{
    // dump("*** Registering -calendar handler.\n");

    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(CLINE_SERVICE_CID,
                                    "Calendar CommandLine Service",
                                    CLINE_SERVICE_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);

    var catman = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(nsICategoryManager);
    catman.addCategoryEntry("command-line-argument-handlers",
                            "calendar command line handler",
                            CLINE_SERVICE_CONTRACTID, true, true);
    catman.addCategoryEntry("command-line-handler", "m-calendar",
                            CLINE_SERVICE_CONTRACTID, true, true);

    // dump("*** Registering text/calendar handler.\n");
    compMgr.registerFactoryLocation(ICALCNT_HANDLER_CID,
                                    "Webcal Content Handler",
                                    ICALCNT_HANDLER_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);
}

CalendarModule.unregisterSelf =
function(compMgr, fileSpec, location)
{
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(CLINE_SERVICE_CID, fileSpec);
    compMgr.unregisterFactoryLocation(ICALCNT_HANDLER_CID, fileSpec);

    catman = Components.classes["@mozilla.org/categorymanager;1"]
                       .getService(nsICategoryManager);
    catman.deleteCategoryEntry("command-line-argument-handlers",
                               "calendar command line handler", true);
    catman.deleteCategoryEntry("command-line-handler",
                               "m-calendar", true);
}

CalendarModule.getClassObject =
function (compMgr, cid, iid) {
    if (cid.equals(CLINE_SERVICE_CID))
        return CLineFactory;

    if (cid.equals(ICALCNT_HANDLER_CID))
        return ICALContentHandlerFactory;

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
