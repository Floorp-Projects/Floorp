/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla calendar code.
 *
 * The Initial Developer of the Original Code is
 *  Michiel van Leeuwen <mvl@exedo.nl>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

//
// calICSCalendar.js
//
// This is a non-sync ics file. It reads the file pointer to by uri when set,
// then writes it on updates. External changes to the file will be
// ignored and overwritten.
//
// XXX Should do locks, so that external changes ore not overwritten.

const calIOperationListener = Components.interfaces.calIOperationListener;
const calICalendar = Components.interfaces.calICalendar;

// calICSCalendar inherits from calMemoryCalendar. This is done in
// createInstance.

function calICSCalendar () {
    this.wrappedJSObject = this;
    this.initICSCalendar();
}

calICSCalendar.prototype = {
    mICSService: null,
    mInitializing: false,
    mObserver: null,

    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.nsIStreamListener))
            return this;
        if (aIID.equals(Components.interfaces.nsIStreamLoaderObserver))
            return this;
        if (aIID.equals(Components.interfaces.nsIInterfaceRequestor))
            return this;
        if (aIID.equals(Components.interfaces.calICalendar))
            return this;
    },
    
    initICSCalendar: function() {
        this.initMemoryCalendar();
        this.mICSService = Components.classes["@mozilla.org/calendar/ics-service;1"]
                                     .getService(Components.interfaces.calIICSService);
        this.mObserver = new calICSObserver(this);
        this.addObserver(this.mObserver, calICalendar.ITEM_FILTER_TYPE_ALL);
    },

    mUri: null,
    get uri() { return this.mUri },
    set uri(aUri) {
        return this.setICSUri(aUri);
    },

    setICSUri: function(aUri) {
        // XXX If this ever uses async io, make sure to also make this
        // async. The ui should not be able to edit the calendar while loading.
        // or just queue the changes from the ui.
        this.mInitializing = true;
        this.mUri = aUri;
        
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                  .getService(Components.interfaces.nsIIOService);

        var channel = ioService.newChannelFromURI(this.mUri);
        channel.loadFlags |= Components.interfaces.nsIRequest.LOAD_BYPASS_CACHE;
        channel.notificationCallbacks = this;

        var streamLoader = Components.classes["@mozilla.org/network/stream-loader;1"]
                                     .createInstance(Components.interfaces.nsIStreamLoader);
        streamLoader.init(channel, this, this);
    },

    // nsIStreamLoaderObserver impl
    // Listener for download. Parse the downlaoded file
    onStreamComplete: function(loader, ctxt, status, resultLength, result)
    {
        // XXX ?? Is this ok?
        this.mItems = new Array();

        var unicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
                                         .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
        // ics files are always utf8
        unicodeConverter.charset = "UTF-8";
        var str = unicodeConverter.convertFromByteArray(result, result.length);
        // Wrap parsing in a try block. Will ignore errors. That's a good thing
        // for non-existing or empty files, but not good for wrong files.
        // you don't want to accidently overwrite them
        // XXX Fix that
        try {
            var calComp = this.mICSService.parseICS(str);
            // XXX also getting VTODO would be nice
            var subComp = calComp.getFirstSubcomponent("VEVENT");
            while (subComp) {
                var event = Components.classes["@mozilla.org/calendar/event;1"]
                                      .createInstance(Components.interfaces.calIEvent);
                event.icalComponent = subComp;
                this.addItem(event, null);

                subComp = calComp.getNextSubcomponent("VEVENT");
            }
        } catch(e) { }
        this.mInitializing = false;
        this.observeOnLoad();
    },

    writeICS: function () {
        var savedthis = this;
        var listener =
        {
            onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail)
            {
                var icsStr = calComp.serializeToICS();

                var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                          .getService(Components.interfaces.nsIIOService);
                var channel = ioService.newChannelFromURI(savedthis.mUri);
                channel.notificationCallbacks = savedthis;
                var uploadChannel = channel.QueryInterface(Components.interfaces.nsIUploadChannel);
                var postStream = Components.classes["@mozilla.org/io/string-input-stream;1"]
                                           .createInstance(Components.interfaces.nsIStringInputStream);
                postStream.setData(icsStr, icsStr.length);
                uploadChannel.setUploadStream(postStream, "text/calendar", -1);
                channel.asyncOpen(savedthis, savedthis);
            },
            onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems)
            {
                var lastmodifed;
                for (var i=0; i<aCount; i++) {
                    calComp.addSubcomponent(aItems[i].icalComponent);
                }
            }
        };

        if (this.mInitializing)
            return;
        if (!this.mUri)
            throw Components.results.NS_ERROR_FAILURE;
dump(">>> WriteICS "+this.mUri.spec+"\n")

        var calComp = this.mICSService.createIcalComponent("VCALENDAR");
        calComp.version = "2.0";
        calComp.prodid = "-//Mozilla.org/NONSGML Mozilla Calendar V1.0//EN";
        this.getItems(calICalendar.ITEM_FILTER_TYPE_ALL, 0, null, null, listener);
    },

    // nsIStreamListener impl
    // For after publishing. For error checks
    
    // XXX use the onError observer calls
    onStartRequest: function(request, ctxt) {},
    onDataAvailable: function(request, ctxt, inStream, sourceOffset, count) {},
    onStopRequest: function(request, ctxt, status, errorMsg)
    {
        ctxt = ctxt.wrappedJSObject;
        var channel;
        try {
            channel = request.QueryInterface(Components.interfaces.nsIHttpChannel);
            dump(channel.requestSucceeded+"\n");
        } catch(e) {
        }

        if (channel && !channel.requestSucceeded) {
            ctxt.observeOnError(null,
                                "Publishing the calendar file failed\n" +
                                  "Status code: "+channel.responseStatus+": "+channel.responseStatusText+"\n");
        }

        else if (!channel && !Components.isSuccessCode(request.status)) {
            ctxt.observeOnError(null,
                                "Publishing the calendar file failed\n" +
                                  "Status code: "+request.status.toString(16)+"\n");
        }
    },

    observeOnLoad: function () {
        for (var i = 0; i < this.mObservers.length; i++)
            this.mObservers[i].observer.onLoad ();
    },

    observeOnError: function (aErrNo, aMessage) {
        for (var i = 0; i < this.mObservers.length; i++)
            this.mObservers[i].observer.onError (aErrNo, aMessage);
    },

    // nsIInterfaceRequestor impl
    getInterface: function(iid, instance) {
        if (iid.equals(Components.interfaces.nsIAuthPrompt)) {
            // use the window watcher service to get a nsIAuthPrompt impl
            return Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                             .getService(Components.interfaces.nsIWindowWatcher)
                             .getNewAuthPrompter(window);
        }
        else if (iid.equals(Components.interfaces.nsIPrompt)) {
            // use the window watcher service to get a nsIPrompt impl
            return Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                             .getService(Components.interfaces.nsIWindowWatcher)
                             .getNewPrompter(window);
        }
        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
    },
};

function calICSObserver(aCalendar) {
    this.mCalendar = aCalendar;
}

calICSObserver.prototype = {
    mCalendar: null,

    // XXX only write when not in a batch!
    onStartBatch: function() {},
    onEndBatch: function() {},
    onLoad: function() {},
    onAddItem: function(aItem) {
        this.mCalendar.writeICS();
    },
    onModifyItem: function(aNewItem, aOldItem) {
        this.mCalendar.writeICS();
    },
    onDeleteItem: function(aDeletedItem) {
        this.mCalendar.writeICS();
    },
    onAlarm: function(aAlarmItem) {},
    onError: function(aMessage) {},
};

/****
 **** module registration
 ****/

var gWiredUpPrototype = false;

var calICSCalendarModule = {

    mCID: Components.ID("{f8438bff-a3c9-4ed5-b23f-2663b5469abf}"),
    mContractID: "@mozilla.org/calendar/calendar;1?type=ics",
    
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.mCID,
                                        "Calendar ICS provider",
                                        this.mContractID,
                                        fileSpec,
                                        location,
                                        type);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.mCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.mFactory;
    },

    mFactory: {
        createInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            if (!gWiredUpPrototype) {
                var memCal = Components.classes["@mozilla.org/calendar/calendar;1?type=memory"]
                                       .createInstance(Components.interfaces.calICalendar);
                calICSCalendar.prototype.__proto__ = memCal.wrappedJSObject.__proto__;
            }
            return (new calICSCalendar()).QueryInterface(iid);
        }
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return calICSCalendarModule;
}
