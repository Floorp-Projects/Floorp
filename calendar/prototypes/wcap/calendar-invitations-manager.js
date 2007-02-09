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
 * The Original Code is Sun Microsystems code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Thomas Benisch <thomas.benisch@sun.com>
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

var gInvitationsRequestManager = null;

function getInvitationsRequestManager() {
    if (!gInvitationsRequestManager) {
        gInvitationsRequestManager = new InvitationsRequestManager();
    }
    return gInvitationsRequestManager;
}

function InvitationsRequestManager() {
    this.mRequestStatusList = {};
}

InvitationsRequestManager.prototype = {

    mRequestStatusList: null,

    getRequestStatus: function(calendar) {
        var calendarId = this._getCalendarId(calendar);
        if (calendarId in this.mRequestStatusList) {
            return this.mRequestStatusList[calendarId];
        }
        return null;
    },

    addRequestStatus: function(calendar, requestStatus) {
        var calendarId = this._getCalendarId(calendar);
        this.mRequestStatusList[calendarId] = requestStatus;
    },

    deleteRequestStatus: function(calendar) {
        var calendarId = this._getCalendarId(calendar);
        if (calendarId in this.mRequestStatusList) {
            delete this.mRequestStatusList[calendarId];
        }
    },

    getPendingRequests: function() {
        var count = 0;
        for each (var requestStatus in this.mRequestStatusList) {
            var request = requestStatus.request;
            if (request && request.isPending) {
                count++;
            }
        }
        return count;
    },

    cancelPendingRequests: function() {
        for each (var requestStatus in this.mRequestStatusList) {
            var request = requestStatus.request;
            if (request && request.isPending) {
                request.cancel(null);
            }
        }
    },

    _getCalendarId: function(calendar) {
        return encodeURIComponent(calendar.uri.spec);
    }
}

var gInvitationsManager = null;

function getInvitationsManager() {
    if (!gInvitationsManager) {
        gInvitationsManager = new InvitationsManager();
    }
    return gInvitationsManager;
}

function InvitationsManager() {
    this.mItemList = new Array();
    this.mOperationListeners = new Array();
    this.mStartDate = null;
    this.mJobsPending = 0;
    this.mTimer = null;
    this.mUnregisteredCalendars = new Array();
    var calendarManagerObserver = {
        mInvitationsManager: this,        
        onCalendarRegistered: function(aCalendar) {
        },
        onCalendarUnregistering: function(aCalendar) {
            this.mInvitationsManager.unregisterCalendar(aCalendar);
        },
        onCalendarDeleting: function(aCalendar) {
        },
        onCalendarPrefSet: function(aCalendar, aName, aValue) {
        },
        onCalendarPrefDeleting: function(aCalendar, aName) {
        }
    };
    getCalendarManager().addObserver(calendarManagerObserver);
}

InvitationsManager.prototype = {

    mItemList: null,
    mOperationListeners: null,
    mStartDate: null,
    mJobsPending: 0,
    mTimer: null,
    mUnregisteredCalendars: null,

    scheduleInvitationsUpdate:
    function(firstDelay, repeatDelay, operationListener) {
        if (this.mTimer) {
            this.mTimer.cancel();
        } else {
            this.mTimer = 
                Components.classes["@mozilla.org/timer;1"]
                          .createInstance(Components.interfaces.nsITimer);
        }
        var callback = {
            mInvitationsManager: this,
            mRepeatDelay: repeatDelay,
            mOperationListener: operationListener,
            notify: function(timer) {
                if (timer.delay != this.mRepeatDelay) {
                    timer.delay = this.mRepeatDelay;
                }
                this.mInvitationsManager.getInvitations(
                    true, this.mOperationListener);
            }
        };
        this.mTimer.initWithCallback(callback, firstDelay,
            this.mTimer.TYPE_REPEATING_SLACK);
    },

    cancelInvitationsUpdate: function() {
        if (this.mTimer) {
            this.mTimer.cancel();
        }
    },

    getInvitations: function(suppressOnError, operationListener1, operationListener2) {
        if (operationListener1) {
            this.addOperationListener(operationListener1);
        }
        if (operationListener2) {
            this.addOperationListener(operationListener2);
        }
        this.updateStartDate();
        var requestManager = getInvitationsRequestManager();
        var calendars = getCalendarManager().getCalendars({});
        for each (var calendar in calendars) {
            try {
                var wcapCalendar = calendar.QueryInterface(
                    Components.interfaces.calIWcapCalendar);
                if (!wcapCalendar.isOwnedCalendar) {
                    continue;
                }
                var listener = {
                    mRequestManager: requestManager,
                    mInvitationsManager: this,
                    QueryInterface: function(aIID) {
                        if (!aIID.equals(Components.interfaces.nsISupports) &&
                            !aIID.equals(Components.interfaces.calIOperationListener) &&
                            !aIID.equals(Components.interfaces.calIObserver)) {
                            throw Components.results.NS_ERROR_NO_INTERFACE;
                        }
                        return this;
                    },
                    // calIOperationListener
                    onOperationComplete:
                    function(aCalendar, aStatus, aOperationType, aId, aDetail) {
                        if (aOperationType != Components.interfaces.calIOperationListener.GET &&
                            aOperationType != Components.interfaces.calIWcapCalendar.SYNC) {
                            return;
                        }
                        var requestStatus =
                            this.mRequestManager.getRequestStatus(aCalendar);
                        if (Components.isSuccessCode(aStatus)) {
                            if (requestStatus.firstRequest) {
                                requestStatus.firstRequest = false;
                                requestStatus.lastUpdate = requestStatus.firstRequestStarted;
                            } else {
                                requestStatus.lastUpdate = aDetail;
                            }
                        }
                        if (this.mRequestManager.getPendingRequests() == 0) {
                            this.mInvitationsManager.deleteUnregisteredCalendarItems();
                            this.mInvitationsManager.mItemList.sort(
                                function (a, b) {
                                    var dateA = a.startDate.getInTimezone(calendarDefaultTimezone());
                                    var dateB = b.startDate.getInTimezone(calendarDefaultTimezone());
                                    return dateA.compare(dateB);
                                });
                            var listener;
                            while ((listener = this.mInvitationsManager.mOperationListeners.shift())) {
                                listener.onGetResult(
                                    null,
                                    Components.results.NS_OK,
                                    Components.interfaces.calIItemBase,
                                    null,
                                    this.mInvitationsManager.mItemList.length,
                                    this.mInvitationsManager.mItemList);
                                listener.onOperationComplete(
                                    null,
                                    Components.results.NS_OK,
                                    Components.interfaces.calIOperationListener.GET,
                                    null,
                                    null );
                            }
                        }
                    },
                    onGetResult:
                    function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
                        if (!Components.isSuccessCode(aStatus)) {
                            return;
                        }
                        for each (var item in aItems) {
                            this.mInvitationsManager.addItem(item);
                        }
                    },
                    // calIObserver
                    onStartBatch: function() {
                    },
                    onEndBatch: function() {
                    },
                    onLoad: function() {
                    },
                    onAddItem: function(aItem) {
                        this.mInvitationsManager.addItem(aItem);
                    },
                    onModifyItem: function(aNewItem, aOldItem) {
                        this.mInvitationsManager.deleteItem(aNewItem);
                        this.mInvitationsManager.addItem(aNewItem);
                    },
                    onDeleteItem: function(aDeletedItem) {
                        this.mInvitationsManager.deleteItem(aDeletedItem);
                    },
                    onError: function(aErrNo, aMessage) {
                    }
                };
                var requestStatus = requestManager.getRequestStatus(wcapCalendar);
                if (!requestStatus) {
                    requestStatus = {
                        request: null,
                        firstRequest: true,
                        firstRequestStarted: null,
                        lastUpdate: null
                    };
                    requestManager.addRequestStatus(wcapCalendar, requestStatus);
                }
                if (!requestStatus.request || !requestStatus.request.isPending) {
                    var filter = (suppressOnError
                        ? wcapCalendar.ITEM_FILTER_SUPPRESS_ONERROR : 0);
                    var request;
                    if (requestStatus.firstRequest) {
                        requestStatus.firstRequestStarted = this.getDate();
                        filter |= wcapCalendar.ITEM_FILTER_REQUEST_NEEDS_ACTION;
                        request = wcapCalendar.wrappedJSObject.getItems(filter,
                            0, this.mStartDate, null, listener);
                    } else {
                        filter |= wcapCalendar.ITEM_FILTER_TYPE_EVENT;
                        request = wcapCalendar.syncChangesTo(null, filter,
                            requestStatus.lastUpdate, listener);
                    }
                    requestStatus.request = request;
                }
            } catch(e) {
            }
        }
        if (requestManager.getPendingRequests() == 0) {
            this.deleteUnregisteredCalendarItems();
            var listener;
            while ((listener = this.mOperationListeners.shift())) {
                listener.onOperationComplete(
                    null,
                    Components.results.NS_ERROR_FAILURE,
                    Components.interfaces.calIOperationListener.GET,
                    null,
                    null );
            }
        }
    },

    openInvitationsDialog: function(onLoadOperationListener, finishedCallBack) {
        var args = new Object();
        args.onLoadOperationListener = onLoadOperationListener;
        args.queue = new Array();
        args.finishedCallBack = finishedCallBack;
        args.requestManager = getInvitationsRequestManager();
        args.invitationsManager = this;
        // the dialog will reset this to auto when it is done loading
        window.setCursor("wait");
        // open the dialog modally
        window.openDialog(
            "chrome://calendar/content/calendar-invitations-dialog.xul",
            "_blank",
            "chrome,titlebar,modal,resizable",
            args);
    },

    processJobQueue: function(queue, jobQueueFinishedCallBack) {
        // TODO: undo/redo
        var operationListener = {
            mInvitationsManager: this,
            mJobQueueFinishedCallBack: jobQueueFinishedCallBack,
            onOperationComplete:
            function(aCalendar, aStatus, aOperationType, aId, aDetail) {
                if (Components.isSuccessCode(aStatus) &&
                    aOperationType == Components.interfaces.calIOperationListener.MODIFY) {
                    this.mInvitationsManager.deleteItem(aDetail);
                    this.mInvitationsManager.addItem(aDetail);
                }
                this.mInvitationsManager.mJobsPending--;
                if (this.mInvitationsManager.mJobsPending == 0 && 
                    this.mJobQueueFinishedCallBack) {
                    this.mJobQueueFinishedCallBack();
                }
            },
            onGetResult:
            function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
            }
        };
        this.mJobsPending = 0;
        for (var i = 0; i < queue.length; i++) {
            var job = queue[i];
            var oldItem = job.oldItem;
            var newItem = job.newItem;
            switch (job.action) {
                case 'modify':
                    this.mJobsPending++;
                    newItem.calendar.modifyItem(
                        newItem, oldItem, operationListener);
                    break;
                default:
                    break;
            }
        }
        if (this.mJobsPending == 0 && jobQueueFinishedCallBack) {
            jobQueueFinishedCallBack();
        }
    },

    hasItem: function(item) {
        for (var i = 0; i < this.mItemList.length; ++i) {
            if (this.mItemList[i].hasSameIds(item)) {
                return true;
            }
        }
        return false;
    },

    addItem: function(item) {
        var recInfo = item.recurrenceInfo;
        if (recInfo && this.getParticipationStatus(item) != "NEEDS-ACTION") {
            var ids = recInfo.getExceptionIds({});
            for each (var id in ids) {
                var ex = recInfo.getExceptionFor(id, false);
                if (ex && this.validateItem(ex) && !this.hasItem(ex)) {
                    this.mItemList.push(ex);
                }
            }
        } else {
            if (this.validateItem(item) && !this.hasItem(item)) {
                this.mItemList.push(item);
            }
        }
    },

    deleteItem: function(item) {
        var i = 0;
        while (i < this.mItemList.length) {
            // delete all items with the same id from the list;
            // if item is a recurrent event, also all exceptions are deleted
            if (this.mItemList[i].id == item.id) {
                this.mItemList.splice(i, 1);
            } else {
                i++;
            }
        }
    },

    addOperationListener: function(operationListener) {
        for each (var listener in this.mOperationListeners) {
            if (listener == operationListener) {
                return false;
            }
        }
        this.mOperationListeners.push(operationListener);
        return true;
    },

    getDate: function() {
        var date = Components.classes["@mozilla.org/calendar/datetime;1"]
                             .createInstance(Components.interfaces.calIDateTime);
        date.jsDate = new Date();
        return date;
    },

    getStartDate: function() {
        var date = Components.classes["@mozilla.org/calendar/datetime;1"]
                             .createInstance(Components.interfaces.calIDateTime);
        date.jsDate = new Date();
        date = date.getInTimezone(calendarDefaultTimezone());
        date.hour = 0;
        date.minute = 0;
        date.second = 0;
        return date;
    },

    updateStartDate: function() {
        if (!this.mStartDate) {
            this.mStartDate = this.getStartDate();
        } else {
            var startDate = this.getStartDate();
            if (startDate.compare(this.mStartDate) > 0) {
                this.mStartDate = startDate;
                var i = 0;
                while (i < this.mItemList.length) {
                    if (!this.validateItem(this.mItemList[i])) {
                        this.mItemList.splice(i, 1);
                    } else {
                        i++;
                    }
                }
            }
        }
    },

    validateItem: function(item) {
        var participationStatus = this.getParticipationStatus(item);
        if (participationStatus != "NEEDS-ACTION") {
            return false;
        }
        if (item.recurrenceInfo) {
            return true;
        } else {
            var startDate = item.startDate.getInTimezone(calendarDefaultTimezone());
            if (startDate.compare(this.mStartDate) >= 0) {
                return true;
            }
        }
        return false;
    },

    getParticipationStatus: function(item) {
        try {
            var wcapCalendar = item.calendar.QueryInterface(
                Components.interfaces.calIWcapCalendar);
            var attendee = wcapCalendar.getInvitedAttendee(item);
            if (attendee)
                return attendee.participationStatus;
        } catch(e) {}
        return null;
    },

    unregisterCalendar: function(calendar) {
        try {
            var wcapCalendar = calendar.QueryInterface(
                Components.interfaces.calIWcapCalendar);
            this.mUnregisteredCalendars.push(wcapCalendar);
        } catch(e) {}
    },

    deleteUnregisteredCalendarItems: function() {
        var calendar;
        while ((calendar = this.mUnregisteredCalendars.shift())) {
            // delete all unregistered calendar items
            var i = 0;
            while (i < this.mItemList.length) {
                if (this.mItemList[i].calendar.uri.equals(calendar.uri)) {
                    this.mItemList.splice(i, 1);
                } else {
                    i++;
                }
            }
            // delete unregistered calendar request status entry
            getInvitationsRequestManager().deleteRequestStatus(calendar);
        }
    }
};
