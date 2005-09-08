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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
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

const kHoursBetweenUpdates = 6;

function newTimerWithCallback(callback, delay, repeating)
{
    var timer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
    
    timer.initWithCallback(callback,
                           delay,
                           (repeating) ? timer.TYPE_REPEATING_PRECISE : timer.TYPE_ONE_SHOT);
    return timer;
}

function jsDateToDateTime(date)
{
    var newDate = Components.classes["@mozilla.org/calendar/datetime;1"].createInstance(Components.interfaces.calIDateTime);
    newDate.jsDate = date;
    return newDate;
}

function jsDateToFloatingDateTime(date)
{
    var newDate = Components.classes["@mozilla.org/calendar/datetime;1"].createInstance(Components.interfaces.calIDateTime);
    newDate.timezone = "floating";
    newDate.year = date.getFullYear();
    newDate.month = date.getMonth();
    newDate.day = date.getDate();
    newDate.hour = date.getHours();
    newDate.minute = date.getMinutes();
    newDate.second = date.getSeconds();
    newDate.normalize();
    return newDate;
}

/*
var testalarmnumber = 0;
function testAlarm(name) {
    this.title = name;
    this.alarmTime = jsDateToDateTime(new Date());
    this.alarmTime.second += 5;
    this.alarmTime.normalize();
    this.calendar = new Object();
    this.calendar.suppressAlarms = false;
    this.id = testalarmnumber++;
}
testAlarm.prototype = {
};
*/


function calAlarmService() {
    this.wrappedJSObject = this;

    this.calendarObserver = {
        alarmService: this,

        onStartBatch: function() { },
        onEndBatch: function() { },
        onLoad: function() { },
        onAddItem: function(aItem) {
            if (aItem.alarmTime)
                this.alarmService.addAlarm(aItem, false);
        },
        onModifyItem: function(aNewItem, aOldItem) {
            this.alarmService.removeAlarm(aOldItem);

            if (aNewItem.alarmTime)
                this.alarmService.addAlarm(aNewItem, false);
        },
        onDeleteItem: function(aDeletedItem) {
            this.alarmService.removeAlarm(aDeletedItem);
        },
        onAlarm: function(aAlarmItem) { },
        onError: function(aErrNo, aMessage) { }
    };


    this.calendarManagerObserver = {
        alarmService: this,

        onCalendarRegistered: function(aCalendar) {
            this.alarmService.observeCalendar(aCalendar);
        },
        onCalendarUnregistering: function(aCalendar) {
            this.alarmService.unobserveCalendar(aCalendar);
        },
        onCalendarDeleting: function(aCalendar) {},
        onCalendarPrefSet: function(aCalendar, aName, aValue) {},
        onCalendarPrefDeleting: function(aCalendar, aName) {}
    };
}

var calAlarmServiceClassInfo = {
    getInterfaces: function (count) {
        var ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calIAlarmService,
            Components.interfaces.nsIObserver,
            Components.interfaces.nsIClassInfo
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function (language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/alarm-service;1",
    classDescription: "Calendar Alarm Service",
    classID: Components.ID("{7a9200dd-6a64-4fff-a798-c5802186e2cc}"),
    implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0
};

calAlarmService.prototype = {
    mRangeStart: null,
    mRangeEnd: null,
    mEvents: {},
    mObservers: [],
    mUpdateTimer: null,
    mStarted: false,

    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.nsIClassInfo))
            return calAlarmServiceClassInfo;

        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIAlarmService) &&
            !aIID.equals(Components.interfaces.nsIObserver))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },


    /* nsIObserver */
    observe: function (subject, topic, data) {
        observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);

        if (topic == "app-startup") {
            observerService.addObserver(this, "profile-after-change", false);
            observerService.addObserver(this, "xpcom-shutdown", false);
        }
        if (topic == "profile-after-change") {
            this.startup();
        }
        if (topic == "xpcom-shutdown") {
            this.shutdown();
        }
    },

    /* calIAlarmService APIs */
    snoozeEvent: function(event, duration) {
        /* modify the event for a new alarm time */
        var alarmTime = jsDateToDateTime((new Date())).getInTimezone("UTC");
        alarmTime.addDuration(duration);
        newEvent = event.clone();
        newEvent.alarmTime = alarmTime;
        // calling modifyItem will cause us to get the right callback
        // and update the alarm properly
        newEvent.calendar.modifyItem(newEvent, event, null);
    },

    addObserver: function(aObserver) {
        dump("observer added\n");
        if (this.mObservers.indexOf(aObserver) != -1)
            return;

        this.mObservers.push(aObserver);
    },

    removeObserver: function(aObserver) {
        dump("observer removed\n");
        function notThis(v) {
            return v != aObserver;
        }

        this.mObservers = this.mObservers.filter(notThis);
    },


    /* helper functions */
    notifyObservers: function(functionName, args) {
        function notify(obs) {
            try { obs[functionName].apply(obs, args);  }
            catch (e) { }
        }
        this.mObservers.forEach(notify);
    },

    startup: function() {
        if (this.mStarted)
            return;

        dump("Starting calendar alarm service\n");
        this.mStarted = true;

        this.calendarManager = Components.classes["@mozilla.org/calendar/manager;1"].getService(Components.interfaces.calICalendarManager);
        var calendarManager = this.calendarManager;
        calendarManager.addObserver(this.calendarManagerObserver);

        var calendars = calendarManager.getCalendars({});
        for each(var calendar in calendars) {
            this.observeCalendar(calendar);
        }

        this.findAlarms();

        /* set up a timer to update alarms every N hours */
        var timerCallback = {
            alarmService: this,
            notify: function(timer) {
                this.alarmService.findAlarms();
            }
        };

        this.mUpdateTimer = newTimerWithCallback(timerCallback, kHoursBetweenUpdates * 3600000, true);

        /* tell people that we're alive so they can start monitoring alarms */
        this.notifier = Components.classes["@mozilla.org/embedcomp/appstartup-notifier;1"].getService(Components.interfaces.nsIObserver);
        var notifier = this.notifier;
        notifier.observe(null, "alarm-service-startup", null);


        /* Test Code
        this.addAlarm(new testAlarm("Meeting with Mr. T"));
        this.addAlarm(new testAlarm("Blah blah"));
        */
    },

    shutdown: function() {
        /* tell people that we're no longer running */
        var notifier = this.notifier;
        notifier.observe(null, "alarm-service-shutdown", null);

        if (this.mUpdateTimer) {
            this.mUpdateTimer.cancel();
            this.mUpdateTimer = null;
        }
        
        var calendarManager = this.calendarManager;
        calendarManager.removeObserver(this.calendarManagerObserver);

        for each(var timer in this.mEvents) {
            timer.cancel();
        }
        this.mEvents = {};

        var calendars = calendarManager.getCalendars({});
        for each(var calendar in calendars) {
            this.unobserveCalendar(calendar);
        }

        this.calendarManager = null;
        this.notifier = null;
        this.mRangeStart = null;
        this.mRangeEnd = null;

        this.mStarted = false;
    },


    observeCalendar: function(calendar) {
        calendar.addObserver(this.calendarObserver);
    },

    unobserveCalendar: function(calendar) {
        calendar.removeObserver(this.calendarObserver);
    },

    addAlarm: function(aItem, skipCheck, alarmTime) {
        // if aItem.alarmTime >= 'now' && aItem.alarmTime <= gAlarmEndTime
        if (!alarmTime)
            alarmTime = aItem.alarmTime.getInTimezone("UTC");

        var now;
        // XXX When the item is floating, should use the default timezone
        // from the prefs, instead of the javascript timezone (which is what
        // jsDateToFloatingDateTime uses)
        if (aItem.alarmTime.timezone == "floating")
            now = jsDateToFloatingDateTime((new Date()));
        else
            now = jsDateToDateTime((new Date())).getInTimezone("UTC");

        var callbackObj = {
            alarmService: this,
            item: aItem,
            notify: function(timer) {
                this.alarmService.alarmFired(this.item);
                delete this.alarmService.mEvents[this.item];
            }
        };

        if ((alarmTime.compare(now) >= 0 && alarmTime.compare(this.mRangeEnd) <= 0) || skipCheck) {
            var timeout = alarmTime.subtractDate(now).inSeconds;
            this.mEvents[aItem.id] = newTimerWithCallback(callbackObj, timeout, false);
            dump("adding alarm timeout (" + timeout + ") for " + aItem + " at " + aItem.alarmTime + "\n");
        }
    },

    removeAlarm: function(aItem) {
        if (aItem.id in this.mEvents) {
            this.mEvents[aItem.id].cancel();
            delete this.mEvents[aItem.id];
        }
    },

    findAlarms: function() {
        if (this.mEvents.length > 0) {
            // This may happen if an alarm is snoozed over the time range mark
            var err = new Error();
            var debug = Components.classes["@mozilla.org/xpcom/debug;1"].getService(Components.interfaces.nsIDebug);
            debug.warning("mEvents.length should always be 0 when we enter findAlarms",
                          err.fileName, err.lineNumber);
        }

        var getListener = {
            alarmService: this,
            onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {
            },
            onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
                for (var i = 0; i < aCount; ++i) {
                    var item = aItems[i];
                    if (item.alarmTime) {
                        this.alarmService.addAlarm(item, false);
                    }
                }
            }
        };

        // figure out the 'now' and 6 hours from now and look for events 
        // between then with alarms
        this.mRangeStart = jsDateToDateTime((new Date())).getInTimezone("UTC");

        var until = this.mRangeStart.clone();
        until.hour += kHoursBetweenUpdates;
        until.normalize();
        this.mRangeEnd = until.getInTimezone("UTC");

        var calendarManager = this.calendarManager;
        var calendars = calendarManager.getCalendars({});
        for each(var calendar in calendars) {
            calendar.getItems(calendar.ITEM_FILTER_TYPE_EVENT,
                              0, this.mRangeStart, this.mRangeEnd, getListener);
        }
    },

    alarmFired: function(event) {
        if (event.calendar.suppressAlarms)
            return;

        this.notifyObservers("onAlarm", [event]);
    }
};
