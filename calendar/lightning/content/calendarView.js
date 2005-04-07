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

/*
  every view needs to have the following:

  properties:
  displayCalendar  calICalendar
  startDate        calIDateTime
  endDate          calIDateTime

  functions:
  goToDay
  goToNext
  goToPrevious
*/

calendarView.prototype = {


    setDisplayCalendar: function(calendar) {
        this.displayCalendar = calendar;
    },

    /* public stuff */
    refresh: function() {
        var savedThis = this;
        var getListener = {
            onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {},
            onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
                for (var i = 0; i < aCount; ++i) {
                    savedThis.createEventBox(aItems[i],
                                             function(a1, a2, a3) {
                                                 eventController.createEventBoxInternal(a1, a2, a3);
                                             });
                }
            }
        };

        var calendar = this.displayCalendar;
        calendar.getItems(calendar.ITEM_FILTER_TYPE_EVENT | calendar.ITEM_FILTER_CLASS_OCCURRENCES,
                          0, this.startDate, this.endDate, getListener);
    },

    goToToday: function() {
        var today = jsDateToDateTime(Date());
        this.goToDay(today);
    },

    goToDay: function(date) {

    },

    goToNext: function() {

    },

    goToPrevious: function() {
    },

    
    /* protected stuff */
    removeElementsByAttribute: function(attributeName, attributeValue) {
        var liveList = document.getElementsByAttribute(attributeName, attributeValue);
        // Delete in reverse order.  Moz1.8+ getElementsByAttribute list is
        // 'live', so when an element is deleted the indexes of later elements
        // change, but in Moz1.7- list is 'dead'.  Reversed order works with both.
        for (var i = liveList.length - 1; i >= 0; i--) {
            var element = liveList.item(i);
            if (element.parentNode != null) 
                element.parentNode.removeChild(element);
        }
    },


    /* private stuff */
    createEventBox: function(aItemOccurrence, aInteralFunction) {
        var startDate;
        var origEndDate;

        if ("displayTimezone" in this) {
            startDate = aItemOccurrence.occurrenceStartDate.getInTimezone(this.displayTimezone).clone();
            origEndDate = aItemOccurrence.occurrenceEndDate.getInTimezone(this.displayTimezone).clone();
        } else {
            // Copy the values from jsDate. jsDate is in de users timezone
            // It's a hack, but it kind of works. It doesn't set the right
            // timezone info for the date, but that's not a real problem.
            startDate = aItemOccurrence.occurrenceStartDate.clone();
            startDate.year = startDate.jsDate.getFullYear();
            startDate.month = startDate.jsDate.getMonth();
            startDate.day = startDate.jsDate.getDate();
            startDate.hour = startDate.jsDate.getHours();
            startDate.minute = startDate.jsDate.getMinutes();
            startDate.second = startDate.jsDate.getSeconds();
            startDate.normalize();
            
            origEndDate = aItemOccurrence.occurrenceEndDate.clone();
            origEndDate.year = origEndDate.jsDate.getFullYear();
            origEndDate.month = origEndDate.jsDate.getMonth();
            origEndDate.day = origEndDate.jsDate.getDate();
            origEndDate.hour = origEndDate.jsDate.getHours();
            origEndDate.minute = origEndDate.jsDate.getMinutes();
            origEndDate.second = origEndDate.jsDate.getSeconds();
            origEndDate.normalize();
        }

        var endDate = startDate.clone();
        endDate.hour = 23;
        endDate.minute = 59;
        endDate.second = 59;
        endDate.normalize();
        while (endDate.compare(origEndDate) < 0) {
            aInteralFunction(aItemOccurrence, startDate, endDate);

            startDate.day = startDate.day + 1;
            startDate.hour = 0;
            startDate.minute = 0;
            startDate.second = 0;
            startDate.normalize();

            endDate.day = endDate.day + 1;
            endDate.normalize();
        }
        aInteralFunction(aItemOccurrence, startDate, origEndDate);
    },
};
