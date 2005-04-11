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

function WeekView()
{
}

WeekView.prototype = {
    __proto__: CalendarView ? (new CalendarView()) : {},

    refresh: function() {
        // clean up anything that was here before
        this.removeElementsByAttribute("eventbox", "weekview");

        // XXX we need to do a lot more work to update column headers, etc here as well.

        this.__proto__.refresh();
    },

    goToDay: function(date) {
        // compute this.startDate and this.endDate from date
        this.startDate = date.clone();
        this.endDate = date.clone();

        this.startDate.day -= this.startDate.weekday;
        this.startDate.hour = this.startDate.minute = this.startDate.second = 0;
        this.startDate.normalize();

        this.endDate.day += (6 - this.endDate.weekday);
        this.endDate.hour = 23;
        this.endDate.minute = 59;
        this.endDate.second = 59;
        this.endDate.normalize();

        this.refresh();
    },

    goToNext: function() {
        var currentEndDate = this.endDate.clone();
        currentEndDate.day += 7;
        currentEndDate.normalize();
        goToDay(currentEndDate);
    },

    goToPrevious: function() {
        var currentEndDate = this.endDate.clone();
        currentEndDate.day -= 7;
        currentEndDate.normalize();
        goToDay(currentEndDate);
    },

};


WeekView.prototype.createEventBoxInternal = function(itemOccurrence, startDate, endDate)
{
    var calEvent = itemOccurrence.item.QueryInterface(Components.interfaces.calIEvent);

    // Check if the event is within the bounds of events to be displayed.
    if ((endDate.jsDate < this.displayStartDate) ||
        (startDate.jsDate > this.displayEndDate))
        return;

    // XXX Should this really be done? better would be to adjust the
    // lowestStart and highestEnd
    if ((endDate.hour < this.lowestStartHour) ||
        (startDate.hour > this.highestEndHour))
        return;

    if (startDate.hour < this.lowestStartHour) {
        startDate.hour = this.lowestStartHour;
        startDate.normalize();
    }
    if (endDate.hour > this.highestEndHour) {
        endDate.hour = this.highestEndHour;
        endDate.normalize();
    }

    dump(startDate+" "+endDate+"\n");
    dump(this.displayEndDate+"\n");

    /*
      if (calEvent.isAllDay) {
      endDate = endDate.clone();
      endDate.hour = 23;
      endDate.minute = 59;
      endDate.normalize();
      }
    */
    debug("all day:   " + calEvent.isAllDay + "\n");
    debug("startdate: " + startDate + "\n");
    debug("enddate:   " + endDate + "\n");

    var startHour = startDate.hour;
    var startMinutes = startDate.minute;
    var eventDuration = (endDate.jsDate - startDate.jsDate) / (60 * 60 * 1000);

    debug("duration:  " + eventDuration + "\n");

    var eventBox = document.createElement("vbox");

    // XXX Consider changing this to only store the ID
    eventBox.event = calEvent;

    var ElementOfRef = document.getElementById("week-tree-day-" + gRefColumnIndex + "-item-" + startHour) ;
    var hourHeight = ElementOfRef.boxObject.height;
    var ElementOfRefEnd = document.getElementById("week-tree-day-" + gRefColumnIndex + "-item-" + endDate.hour) ;
    var hourHeightEnd = ElementOfRefEnd.boxObject.height;
   
    var hourWidth = ElementOfRef.boxObject.width;
    var eventSlotWidth = Math.round(hourWidth / 1/*calendarEventDisplay.totalSlotCount*/);
   
    var Width = ( 1 /*calendarEventDisplay.drawSlotCount*/ * eventSlotWidth ) - 1;
    eventBox.setAttribute( "width", Width );

    var top = eval( ElementOfRef.boxObject.y + ( ( startMinutes/60 ) * hourHeight ) );
    top = top - ElementOfRef.parentNode.boxObject.y - 2;
    eventBox.setAttribute("top", top);

    var bottom = eval( ElementOfRefEnd.boxObject.y + ( ( endDate.minute/60 ) * hourHeightEnd ) );
    bottom = bottom - ElementOfRefEnd.parentNode.boxObject.y - 2;
    eventBox.setAttribute("height", bottom - top);

    // figure out what column we need to put this on
    debug("d: "+gHeaderDateItemArray[1].getAttribute("date")+"\n");
    var dayIndex = new Date(gHeaderDateItemArray[1].getAttribute("date"));
    var index = startDate.weekday - dayIndex.getDay();
    debug("index is:" + index + "(" + startDate.weekday + " - " + dayIndex.getDay() + ")\n");

    var boxLeft = document.getElementById("week-tree-day-"+index+"-item-"+startHour).boxObject.x - 
                  document.getElementById( "week-view-content-box" ).boxObject.x +
                  ( /*calendarEventDisplay.startDrawSlot*/0 * eventSlotWidth );
    //dump(boxLeft + "\n");
    eventBox.setAttribute("left", boxLeft);
   
    // set the event box to be of class week-view-event-class and the appropriate calendar-color class
    this.setEventboxClass(eventBox, calEvent, "week-view");
  
    eventBox.setAttribute("eventbox", "weekview");
    eventBox.setAttribute("dayindex", index + 1);
    eventBox.setAttribute("onclick", "weekEventItemClick(this, event)" );
    eventBox.setAttribute("ondblclick", "weekEventItemDoubleClick(this, event)");
    eventBox.setAttribute("ondraggesture", "nsDragAndDrop.startDrag(event,calendarViewDNDObserver);");
    eventBox.setAttribute("ondragover", "nsDragAndDrop.dragOver(event,calendarViewDNDObserver)");
    eventBox.setAttribute("ondragdrop", "nsDragAndDrop.drop(event,calendarViewDNDObserver)");
    eventBox.setAttribute("id", "week-view-event-box-" + calEvent.id);
    eventBox.setAttribute("name", "week-view-event-box-" + calEvent.id);
    eventBox.setAttribute("onmouseover", "getEventToolTip(this, event)" );
    eventBox.setAttribute("tooltip", "eventTooltip");

    // Event box text (title, location and description)
    if (calEvent.title || calEvent.getProperty("location")) {
        var titleText = ( (calEvent.title || "") + 
                          (calEvent.getProperty("location") ? " ("+calEvent.getProperty("location")+")" : "") );
        var titleElement = document.createElement( "label" );
        titleElement.setAttribute( "class", "week-view-event-title-label-class" );
        titleElement.appendChild( document.createTextNode( titleText ));
        eventBox.appendChild( titleElement );
    }
    if (calEvent.getProperty("description")) {
        var descriptionElement = document.createElement( "description" );
        descriptionElement.setAttribute( "class", "week-view-event-description-class" );
        descriptionElement.appendChild( document.createTextNode( calEvent.getProperty("description") ));
        eventBox.appendChild( descriptionElement );
    }

    debug("Adding eventBox " + eventBox + "\n");
    document.getElementById("week-view-content-board").appendChild(eventBox);
}
