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
// This is a non-sync ics file. It reads the file pointer to bu uri once,
// then writes it on updates. External changes to the file will be
// ignored and overwritten.
//

const calIOperationListener = Components.interfaces.calIOperationListener;
const calICalendar = Components.interfaces.calICalendar;

function calICSCalendar () {
    this.wrappedJSObject = this;
    this.mMemoryCalendar = Components.classes["@mozilla.org/calendar/calendar;1?type=memory"]
                                     .createInstance(Components.interfaces.calICalendar);
    this.mICSService = Components.classes["@mozilla.org/calendar/ics-service;1"]
                                 .getService(Components.interfaces.calIICSService);
    this.mObserver = new calICSObserver(this);
    this.mMemoryCalendar.addObserver(this.mObserver, calICalendar.ITEM_FILTER_TYPE_ALL);
}

calICSCalendar.prototype = {
    mMemoryCalendar: null,
    mUri: null,
    mICSService: null,
    mInitializing: false,
  
    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calICalendar))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },


    get uri() { return this.mUri },
    set uri(aUri) {
        // Need this listener later, to add the items.
        var emptyListener =
        {
          onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {},
          onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {}
        };

        // XXX If this ever uses async io, make sure to also make this
        // async. The ui should not be able to edit the calendar while loading
        this.mInitializing = true;

        this.mUri = aUri;
        var file = Components.classes["@mozilla.org/file/local;1"]
                             .createInstance(Components.interfaces.nsILocalFile);
        file.initWithPath(this.mUri.path);
        var fileIS = Components.classes["@mozilla.org/network/file-input-stream;1"]
                               .createInstance(Components.interfaces.nsIFileInputStream);
        // Wrap in a try block, in case the file isn't found. This isn't fatal
        // XXX other errors should be fatal!
        try {
            fileIS.init(file, -1, -1, 0);
            var inputStream = Components.classes["@mozilla.org/scriptableinputstream;1"]
                                        .createInstance(Components.interfaces.nsIScriptableInputStream);
            inputStream.init(fileIS);
            var str = inputStream.read(-1);
            fileIS.close();

            var calComp = this.mICSService.parseICS(str);
            var subComp = calComp.getFirstSubcomponent("VEVENT");
            while (subComp) {
                var event = Components.classes["@mozilla.org/calendar/event;1"]
                                      .createInstance(Components.interfaces.calIEvent);
                event.icalComponent = subComp;
                this.mMemoryCalendar.addItem(event, emptyListener);

                subComp = calComp.getNextSubcomponent("VEVENT");
            }
        } catch(e) {
        }
        this.mInitializing = false;
    },

    // void addObserver( in calIObserver observer, in unsigned long aItemFilter );
    addObserver: function (aObserver, aItemFilter) {
        return this.mMemoryCalendar.addObserver(aObserver, aItemFilter);
    },

    // void removeObserver( in calIObserver observer );
    removeObserver: function (aObserver) {
        return this.mMemoryCalendar.removeObserver(aObserver);
    },

    // void modifyItem( in calIItemBase aItem, in calIOperationListener aListener );
    modifyItem: function (aItem, aListener) {
        var listener = new calICSListener(this, aListener);
        this.mMemoryCalendar.modifyItem(aItem, listener);
    },

    // void deleteItem( in string id, in calIOperationListener aListener );
    deleteItem: function (aItem, aListener) {
        var listener = new calICSListener(this, aListener);
        this.mMemoryCalendar.deleteItem(aItem, listener);
    },

    // void addItem( in calIItemBase aItem, in calIOperationListener aListener );
    addItem: function (aItem, aListener) {
        // XXX Need to set the of the new item.
        // can't set it here, because memoy calendar clones the item,
        // and doesn't return the clone.
        var listener = new calICSListener(this, aListener);
        this.mMemoryCalendar.addItem(aItem, listener);
    },

    // void getItem( in string aId, in calIOperationListener aListener );
    getItem: function (aId, aListener) {
        var listener = new calICSListener(this, aListener);
        this.mMemoryCalendar.getItem(aId, listener);
    },

    // void getItems( in unsigned long aItemFilter, in unsigned long aCount, 
    //                in calIDateTime aRangeStart, in calIDateTime aRangeEnd,
    //                in calIOperationListener aListener );
    getItems: function (aItemFilter, aCount, aRangeStart, aRangeEnd, aListener) {
        var listener = new calICSListener(this, aListener);
        this.mMemoryCalendar.getItems(aItemFilter, aCount, aRangeStart, aRangeEnd, listener);
    },

    writeICS: function () {
        var savedthis = this;
        var listener =
        {
            onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail)
            {
                if (!savedthis.mUri)
                     throw Components.results.NS_ERROR_FAILURE;

                var icsStr = calComp.serializeToICS();
                var file = Components.classes["@mozilla.org/file/local;1"]
                                     .createInstance(Components.interfaces.nsILocalFile);
                file.initWithPath(savedthis.mUri.path);
                var fileOS = Components.classes["@mozilla.org/network/file-output-stream;1"]
                                       .createInstance(Components.interfaces.nsIFileOutputStream);
                fileOS.init(file, -1, -1, 0);
                fileOS.write(icsStr, icsStr.length);
                fileOS.close();
            },
            onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems)
            {
                for (var i=0; i<aCount; i++) {
                    calComp.addSubcomponent(aItems[i].icalComponent);
                }
            }
        };

        var calComp = this.mICSService.createIcalComponent("VCALENDAR");
        calComp.version = "2.0";
        calComp.prodid = "-//Mozilla.org/NONSGML Mozilla Calendar V1.0//EN";
        this.mMemoryCalendar.getItems(calICalendar.ITEM_FILTER_TYPE_ALL, 0, null, null, listener);
    }

};

// This wrapper is needed to convert the calendar parameter from the
// memory to this.
function calICSListener(aCalendar, aListener) {
    this.mCalendar = aCalendar;
    this.mListener = aListener;
}

calICSListener.prototype = {
    mCalendar: null,
    mListener: null,

    onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {
        if (this.mListener)
            this.mListener.onOperationComplete(this.mCalendar, aStatus, aOperationType, aId, aDetail)
    },
    onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
        if (this.mListener)
            this.mListener.onGetResult(this.mCalendar, aStatus, aItemType, aDetail, aCount, aItems)
    }
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
