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
 *   Robin Edrenius <robin.edrenius@gmail.com>
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


/* all params are optional */
function createEventWithDialog(calendar, startDate, endDate, summary, event)
{
    const kDefaultTimezone = calendarDefaultTimezone();


    var onNewEvent = function(event, calendar, originalEvent) {
        doTransaction('add', event, calendar, null, null);
    }

    if (event) {
        openEventDialog(event, calendar, "new", onNewEvent, null);
        return;
    }
    
    event = createEvent();

    if (!startDate) {
        // Have we shown the calendar view yet? (Lightning)
        if (currentView().initialized) {
            startDate = currentView().selectedDay.clone();
        } else {
            startDate = jsDateToDateTime(new Date()).getInTimezone(kDefaultTimezone);
        }
        startDate.isDate = true;
    }

    if (startDate.isDate) {
        if (!startDate.isMutable) {
            startDate = startDate.clone();
        }
        startDate.isDate = false;
        startDate.hour = now().hour;
        startDate.minute = 0;
        startDate.second = 0;
        startDate.normalize();
   }

    event.startDate = startDate.clone();

    if (!endDate) {
        endDate = startDate.clone();
        endDate.minute += getPrefSafe("calendar.event.defaultlength", 60);
        endDate.normalize();
    }
    event.endDate = endDate.clone();

    if (calendar) {
        event.calendar = calendar;
    } else if ("getSelectedCalendarOrNull" in window) {
        // Sunbird specific code
        event.calendar = getSelectedCalendarOrNull();
    }

    if (summary)
        event.title = summary;

    setDefaultAlarmValues(event);

    openEventDialog(event, calendar, "new", onNewEvent, null);
}

function createTodoWithDialog(calendar, dueDate, summary, todo)
{
    const kDefaultTimezone = calendarDefaultTimezone();

    var onNewItem = function(item, calendar, originalItem) {
        doTransaction('add', item, calendar, null, null);
    }

    if (todo) {
        openEventDialog(todo, calendar, "new", onNewItem, null);
        return;
    }

    todo = createTodo();

    if (calendar) {
        todo.calendar = calendar;
    } else if ("getSelectedCalendarOrNull" in window) {
        // Sunbird specific code
        todo.calendar = getSelectedCalendarOrNull();
    }

    if (summary)
        todo.title = summary;

    if (dueDate)
        todo.dueDate = dueDate;

    var onNewItem = function(item, calendar, originalItem) {
        calendar.addItem(item, null);
    }

    setDefaultAlarmValues(todo);

    openEventDialog(todo, calendar, "new", onNewItem, null);
}


function modifyEventWithDialog(item, job)
{
    var onModifyItem = function(item, calendar, originalItem) {
        // compare cal.uri because there may be multiple instances of
        // calICalendar or uri for the same spec, and those instances are
        // not ==.
        if (!originalItem.calendar || 
            (originalItem.calendar.uri.equals(calendar.uri)))
            doTransaction('modify', item, item.calendar, originalItem, null);
        else {
            doTransaction('move', item, calendar, originalItem, null);
        }
    }

    if (item) {
        openEventDialog(item, item.calendar, "modify", onModifyItem, job);
    }
}

function openEventDialog(calendarItem, calendar, mode, callback, job)
{
    var args = new Object();
    args.calendarEvent = calendarItem;
    args.calendar = calendar;
    args.mode = mode;
    args.onOk = callback;
    args.job = job;

    // the dialog will reset this to auto when it is done loading.
    window.setCursor("wait");

    // open the dialog modeless
    var url = "chrome://calendar/content/calendar-event-dialog.xul";

    if (getPrefSafe("calendar.prototypes.wcap", false)) {
      url = "chrome://calendar/content/sun-calendar-event-dialog.xul";
    }

    openDialog(url, "_blank", "chrome,titlebar,resizable", args);
}

// When editing a single instance of a recurring event, we need to figure out
// whether the user wants to edit all instances, or just this one.  This
// function prompts this question (if the item is actually an instance of a
// recurring event) and returns the appropriate item that should be modified.
// Returns null if the prompt was cancelled.
function getOccurrenceOrParent(occurrence) {
    // Check if this actually is an instance of a recurring event
    if (occurrence == occurrence.parentItem) {
        return occurrence;
    }

    // if the user wants to edit an occurrence which is already
    // an exception, always edit this single item.
    var parentItem = occurrence.parentItem;
    var rec = parentItem.recurrenceInfo;
    if (rec) {
        var exceptions = rec.getExceptionIds({});
        if (exceptions.some(function (exid) {
                                return exid.compare(occurrence.recurrenceId) == 0;
                            })) {
            return occurrence;
        }
    }

    var promptService = 
             Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                       .getService(Components.interfaces.nsIPromptService);

    var promptTitle = calGetString("calendar", "editRecurTitle");
    var promptMessage = calGetString("calendar", "editRecurMessage");
    var buttonLabel1 = calGetString("calendar", "editRecurAll");
    var buttonLabel2 = calGetString("calendar", "editRecurSingle");

    var flags = promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0 +
                promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1 +
                promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_2;

    var choice = promptService.confirmEx(null, promptTitle, promptMessage, flags,
                                         buttonLabel1,null , buttonLabel2, null, {});
    switch(choice) {
        case 0: return occurrence.parentItem;
        case 2: return occurrence;
        default: return null;
    }
}

/**
 * Read default alarm settings from user preferences and apply them to
 * the event/todo passed in.
 *
 * @param aItem   The event or todo the settings should be applied to.
 */
function setDefaultAlarmValues(aItem)
{
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefService);
    var alarmsBranch = prefService.getBranch("calendar.alarms.");

    if (isEvent(aItem)) {
        try {
            if (alarmsBranch.getIntPref("onforevents") == 1) {
                var alarmOffset = Components.classes["@mozilla.org/calendar/duration;1"]
                                            .createInstance(Components.interfaces.calIDuration);
                try {
                    var units = alarmsBranch.getCharPref("eventalarmunit");
                    alarmOffset[units] = alarmsBranch.getIntPref("eventalarmlen");
                    alarmOffset.isNegative = true;
                } catch(ex) {
                    alarmOffset.minutes = 15;
                }
                aItem.alarmOffset = alarmOffset;
                aItem.alarmRelated = Components.interfaces.calIItemBase.ALARM_RELATED_START;
            }
        } catch (ex) {
            Components.utils.reportError(
                "Failed to apply default alarm settings to event: " + ex);
        }
    } else if (isToDo(aItem)) {
        try {
            if (alarmsBranch.getIntPref("onfortodos") == 1) {
                // You can't have an alarm if the entryDate doesn't exist.
                if (!aItem.entryDate) {
                    aItem.entryDate = getSelectedDay().clone();
                }
                var alarmOffset = Components.classes["@mozilla.org/calendar/duration;1"]
                                            .createInstance(Components.interfaces.calIDuration);
                try {
                    var units = alarmsBranch.getCharPref("todoalarmunit");
                    alarmOffset[units] = alarmsBranch.getIntPref("todoalarmlen");
                    alarmOffset.isNegative = true;
                } catch(ex) {
                    alarmOffset.minutes = 15;
                }
                aItem.alarmOffset = alarmOffset;
                aItem.alarmRelated = Components.interfaces.calIItemBase.ALARM_RELATED_START;
            }
        } catch (ex) {
            Components.utils.reportError(
                "Failed to apply default alarm settings to task: " + ex);
        }
    }
}

// Undo/Redo code
var gTransactionMgr = Components.classes["@mozilla.org/transactionmanager;1"]
                                .createInstance(Components.interfaces.nsITransactionManager);
function doTransaction(aAction, aItem, aCalendar, aOldItem, aListener) {
    var txn = new calTransaction(aAction, aItem, aCalendar, aOldItem, aListener);
    gTransactionMgr.doTransaction(txn);
    updateUndoRedoMenu();
}

function undo() {
    gTransactionMgr.undoTransaction();
    updateUndoRedoMenu();
}

function redo() {
    gTransactionMgr.redoTransaction();
    updateUndoRedoMenu();
}

function startBatchTransaction() {
    gTransactionMgr.beginBatch();
}
function endBatchTransaction() {
    gTransactionMgr.endBatch();
    updateUndoRedoMenu();
}

function canUndo() {
    return (gTransactionMgr.numberOfUndoItems > 0);
}
function canRedo() {
    return (gTransactionMgr.numberOfRedoItems > 0);
}

// Valid values for aAction: 'add', 'modify', 'delete', 'move'
// aOldItem is only needed for aAction == 'modify'
function calTransaction(aAction, aItem, aCalendar, aOldItem, aListener) {
    this.mAction = aAction;
    this.mItem = aItem;
    this.mCalendar = aCalendar;
    this.mOldItem = aOldItem;
    this.mListener = aListener;
}

calTransaction.prototype = {
    mAction: null,
    mItem: null,
    mCalendar: null,
    mOldItem: null,
    mOldCalendar: null,
    mListener: null,
    mIsDoTransaction: false,

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.nsITransaction) &&
            !aIID.equals(Components.interfaces.calIOperationListener))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {
        if (aStatus == Components.results.NS_OK) {
            if (aOperationType == Components.interfaces.calIOperationListener.ADD ||
                aOperationType ==Components.interfaces.calIOperationListener.MODIFY) {
                // Add/Delete return the original item as detail for success
                if (this.mIsDoTransaction) {
                  this.mItem = aDetail;
                } else {
                  this.mOldItem = aDetail;
                }
            }
        } else {
            Components.utils.reportError("Severe error in internal transaction code!\n" +
                                         aDetail + '\n'+
                                         "Please report this to the developers.\n");
        }
        if (this.mListener) {
            this.mListener.onOperationComplete(aCalendar, aStatus, aOperationType, aId, aDetail);
        }
    },
    onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
        if (this.mListener) {
            this.mListener.onGetResult(aCalendar, aStatus, aItemType, aDetail, aCount, aItems);
        }
    },

    doTransaction: function () {
        this.mIsDoTransaction = true;
        switch (this.mAction) {
            case 'add':
                this.mCalendar.addItem(this.mItem, this);
                break;
            case 'modify':
                this.mCalendar.modifyItem(this.mItem, this.mOldItem,
                                          this);
                break;
            case 'delete':
                this.mCalendar.deleteItem(this.mItem, this);
                break;
            case 'move':
                this.mOldCalendar = this.mOldItem.calendar;
                this.mOldCalendar.deleteItem(this.mOldItem, this);
                this.mCalendar.addItem(this.mItem, this);
                break;
        }
    },
    undoTransaction: function () {
        this.mIsDoTransaction = false;
        switch (this.mAction) {
            case 'add':
                this.mCalendar.deleteItem(this.mItem, this);
                break;
            case 'modify':
                this.mCalendar.modifyItem(this.mOldItem, this.mItem, this);
                break;
            case 'delete':
                this.mCalendar.addItem(this.mItem, this);
                break;
            case 'move':
                this.mCalendar.deleteItem(this.mItem, this);
                this.mOldCalendar.addItem(this.mOldItem, this);
                break;
        }
    },
    redoTransaction: function () {
        this.doTransaction();
    },
    isTransient: false,
    
    merge: function (aTransaction) {
        // No support for merging
        return false;
    }
}
