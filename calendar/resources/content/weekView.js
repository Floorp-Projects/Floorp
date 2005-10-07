/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Mike Potter <mikep@oeone.com>
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Eric Belhaire <belhaire@ief.u-psud.fr> 
 *                 Stuart Parmenter <pavlov@pavlov.net>
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

/*-----------------------------------------------------------------
*   WeekView Class, subclass of CalendarView
*
*   PROPERTIES
*      gHeaderDateItemArray   - Array of text boxes used to display the dates 
*                               of the currently displayed week.
*   

*/
var gDebugEnabled=false;


var gRefColumnIndex = 0;

// Make WeekView inherit from CalendarView
WeekView.prototype = new CalendarView();
WeekView.prototype.constructor = WeekView;

/**
*   WeekView Constructor.
*   
*   PARAMETERS
*      calendarWindow     - the owning instance of CalendarWindow.
*   
*/
function WeekView( calendarWindow )
{
    this.superConstructor( calendarWindow );
   
    //set the time on the left hand side labels
    //need to do this in JavaScript to preserve formatting
    for (var i = 0; i < 24; i++) {
        var TimeToFormat = new Date();
        TimeToFormat.setHours( i );
        TimeToFormat.setMinutes( "0" );
      
        var FormattedTime = calendarWindow.dateFormater.getFormatedTime( TimeToFormat );
      
        var Label = document.getElementById( "week-view-hour-" + i );
        Label.setAttribute( "value", FormattedTime );
    }
   
    // get week view header items
    gHeaderDateItemArray = new Array();
    for (var dayIndex = 1; dayIndex <= 7; ++dayIndex) {
        var headerDateItem = document.getElementById( "week-header-date-" + dayIndex );
        gHeaderDateItemArray[ dayIndex ] = headerDateItem;
    }
   
   var weekViewEventSelectionObserver = 
   {
      onSelectionChanged: function( EventSelectionArray ) {
          if( EventSelectionArray.length > 0 ) {
             //for some reason, this function causes the tree to go into a select / unselect loop
             //putting it in a settimeout fixes this.
             setTimeout( "gCalendarWindow.weekView.clearSelectedDate();", 1 );
            
             var i = 0;
             for( i = 0; i < EventSelectionArray.length; i++ ) {
                 gCalendarWindow.weekView.selectBoxForEvent( EventSelectionArray[i] );
             }
          } else {
             //select the proper day
             gCalendarWindow.weekView.hiliteSelectedDate();
          }
      }
   }
    calendarWindow.EventSelection.addObserver( weekViewEventSelectionObserver );
}

/** PUBLIC
*
*   Redraw the events for the current week
*/
WeekView.prototype.refreshEvents = function()
{
    // clean up anything that was here before
    this.removeElementsByAttribute("eventbox", "weekview");
    this.eventList = new Array();


    // Figure out the start and end days for the week we're currently viewing
    this.displayStartDate = new Date(gHeaderDateItemArray[1].getAttribute("date"));
    this.displayEndDate = new Date(this.displayStartDate);
    this.displayEndDate.setDate(this.displayEndDate.getDate() + 7);

    // Save this off so we can get it again in onGetResult below
    var eventController = this;
    var getListener = {
        onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {
            debug("onOperationComplete\n");
            eventController.drawEventBoxes();
        },
        onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
            for (var i = 0; i < aCount; ++i) {
                eventController.createEventBox(aItems[i],
                                               function(a1, a2, a3) { eventController.addToDisplayList(a1, a2, a3); } );
            }
        }
    };

    var ccalendar = getDisplayComposite();

    var start = jsDateToDateTime(this.displayStartDate).getInTimezone(calendarDefaultTimezone());
    var end = jsDateToDateTime(this.displayEndDate).getInTimezone(calendarDefaultTimezone());

    dump("Fetching events from " + start + " to " + end + "\n");

    ccalendar.getItems(ccalendar.ITEM_FILTER_TYPE_EVENT | ccalendar.ITEM_FILTER_CLASS_OCCURRENCES,
                       0, start, end, getListener);

    return;











/*

  var isOnlyWorkDays = (gOnlyWorkdayChecked == "true");
  var isDayOff = (isOnlyWorkDays? this.preferredDaysOff() : null);

   this.kungFooDeathGripOnEventBoxes = new Array();

   for( var dayIndex = 1; dayIndex <= 7; ++dayIndex ) {
      var headerDateItem = document.getElementById( "week-header-date-" + dayIndex );
      headerDateItem.numEvents = 0;
   }
   
   //get the all day box.
   var AllDayRow = document.getElementById( "week-view-allday-row" ) ;
   AllDayRow.setAttribute( "collapsed", "true" );
   
   //expand the day's content box by setting allday to false..
   document.getElementById( "week-view-content-box" ).removeAttribute( "allday" );
   var allDayExist = false ;
      
   var allDayEventList = new Array();
   var normalEventList = new Array();   
   var eventList = new Array();

   var i;   
   for ( dayIndex = 1; dayIndex <= 7; ++dayIndex ) {
      // get the events for the day and loop through them
      var dayToGet = new Date( gHeaderDateItemArray[dayIndex].getAttribute( "date" ) );
      var dayToGetDay = dayToGet.getDay() ;
      if (isOnlyWorkDays && isDayOff[dayToGetDay]) {
         continue;
      }
      
      eventList[dayIndex] = gEventSource.getEventsForDay( dayToGet );
      allDayEventList[dayIndex] = new Array();
      normalEventList[dayIndex] = new Array();
      
      // get limits for current day
      var limits = this.getViewLimits(eventList[dayIndex], dayToGet);
      
      if( limits.startHour < LowestStartHour ) {
         LowestStartHour = limits.startHour;
      }
      if( limits.endHour > HighestEndHour ) {
         HighestEndHour = limits.endHour;
      }
      //divide events into allday and normal (non-allday) events
      for ( i = 0; i < eventList[dayIndex].length; i++ ) {
         if( eventList[dayIndex][i].event.allDay == true ) {
            allDayEventList[dayIndex].push(eventList[dayIndex][i]); 
            allDayExist = true;
         } else {
            normalEventList[dayIndex].push(eventList[dayIndex][i]); 
         }
      }
   }
   
   if ( allDayExist == true ) {
      //show the all day box 
      AllDayRow.removeAttribute( "collapsed" );
      
      //shrink the day's content box.
      document.getElementById( "week-view-content-box" ).setAttribute( "allday", "true" );
   }
   

   //START FOR LOOP FOR DAYS---> 
   for ( dayIndex = 1; dayIndex <= 7; ++dayIndex ) {
      dayToGet = new Date( gHeaderDateItemArray[dayIndex].getAttribute( "date" ) );
      dayToGetDay = dayToGet.getDay() ;
      if (isOnlyWorkDays && isDayOff[dayToGetDay]) {
        continue;
      }
     
      // Calculate event draw properties (where events are drawn on view)
      this.setDrawProperties(normalEventList[dayIndex]);
      
      // Add non-allday events to DOM
      for ( i = 0; i < normalEventList[dayIndex].length; ++i )
      {
         var eventBox = this.createEventBox( normalEventList[dayIndex][i] );
         document.getElementById( "week-view-content-board" ).appendChild( eventBox );
         this.kungFooDeathGripOnEventBoxes.push( eventBox );
      }
      
      //
      //Everything below this point is old code that will be changed in the near future.
      //It only deals with allday events
      //
      var ThisDayAllDayBox = document.getElementById( "all-day-content-box-week-"+dayIndex );
      while ( ThisDayAllDayBox.hasChildNodes() ) {
         ThisDayAllDayBox.removeChild( ThisDayAllDayBox.firstChild );
      }
      
      for ( var eventIndex = 0; eventIndex < eventList[dayIndex].length; ++eventIndex ) {
         var calendarEventDisplay = eventList[dayIndex][ eventIndex ];
         
         // get the day box for the calendarEvent's day
         var weekBoxItem = gHeaderDateItemArray[ dayIndex ];
         
         // Display no more than three, show dots for the events > 3
         if ( calendarEventDisplay.event.allDay != true ) {
            weekBoxItem.numEvents += 1;
         }
         
         //if its an all day event, don't show it in the hours bulletin board.
         if ( calendarEventDisplay.event.allDay == true ) {
            // build up the text to show for this event
            var eventText = calendarEventDisplay.event.title;
            if( calendarEventDisplay.event.location ) {
               eventText += " " + calendarEventDisplay.event.location;
            }
            
            if( calendarEventDisplay.event.description ) {
               eventText += " " + calendarEventDisplay.event.description;
            }
            
            //note the use of the WeekViewAllDayText Attribute.
            //This is used to remove the text when the day is changed.
            var newHTMLNode = document.createElement( "label" );
            newHTMLNode.setAttribute( "crop", "end" );
            newHTMLNode.setAttribute( "flex", "1" );
            newHTMLNode.setAttribute( "value", eventText );
            newHTMLNode.setAttribute( "WeekViewAllDayText", "true" );
            newHTMLNode.calendarEventDisplay = calendarEventDisplay;
            newHTMLNode.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
            newHTMLNode.setAttribute( "onclick", "weekEventItemClick( this, event )" );
            newHTMLNode.setAttribute( "ondblclick", "weekEventItemDoubleClick( this, event )" );
            newHTMLNode.setAttribute( "tooltip", "gridOccurrenceTooltip" );
         
            var newImage = document.createElement("image");
            newImage.setAttribute( "class", "all-day-event-class" );
            newImage.setAttribute( "WeekViewAllDayText", "true" );
            newImage.calendarEventDisplay = calendarEventDisplay;
            newImage.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
            newImage.setAttribute( "onclick", "weekEventItemClick( this, event )" );
            newImage.setAttribute( "ondblclick", "weekEventItemDoubleClick( this, event )" );
            newImage.setAttribute( "tooltip", "gridOccurrenceTooltip" );
            
            ThisDayAllDayBox.appendChild( newImage );
            ThisDayAllDayBox.appendChild( newHTMLNode );
         }
         
         if( this.calendarWindow.EventSelection.isSelectedEvent( calendarEventDisplay.event ) ) {
            this.selectBoxForEvent( calendarEventDisplay.event );
         }
      }
   } //--> END THE FOR LOOP FOR THE WEEK VIEW
*/
}


WeekView.prototype.addToDisplayList = function(itemOccurrence, startDate, endDate)
{
    this.eventList.push({event:itemOccurrence, start:startDate.clone(), end:endDate.clone()});
}

WeekView.prototype.drawEventBoxes = function()
{
    //initialize view limits from prefs
    this.lowestStartHour = getIntPref(this.calendarWindow.calendarPreferences.calendarPref, "event.defaultstarthour", 8);
    this.highestEndHour = getIntPref(this.calendarWindow.calendarPreferences.calendarPref, "event.defaultendhour", 17);
    for each (event in this.eventList) {
        if (!(event.start.isDate)) { 
             if(event.end.hour > this.highestEndHour)
                 this.highestEndHour = event.end.hour;
             if(event.start.hour < this.lowestStartHour)
                 this.lowestStartHour = event.start.hour;
        }
    }

    //now hide those that aren't applicable
    for (var i = 0; i < 24; i++) {
        document.getElementById("week-view-row-"+i).removeAttribute("collapsed");
    }
    for (i = 0; i < this.lowestStartHour; i++) {
        document.getElementById("week-view-row-"+i).setAttribute("collapsed", "true");
    }
    for (i = (this.highestEndHour + 1); i < 24; i++) {
        document.getElementById("week-view-row-"+i ).setAttribute("collapsed", "true");
    }

    // Need to split in seperate lists for each day.
    var lists = new Array();
    for (var i=0; i<7; ++i) {
        lists[i] = new Array();
    }
    var event;
    for each (event in this.eventList) {
        lists[event.start.weekday].push(event);
    }
    
    
    for (var i=0; i<7; ++i) {
        this.setDrawProperties(lists[i]);
        for each (event in lists[i]) {
            this.createEventBoxInternal(event);
        }
    }
}

/** PRIVATE
*
*   This creates an event box for the week view
*/

WeekView.prototype.createEventBoxInternal = function (event)
{    
    var itemOccurrence = event.event;
    var startDate = event.start;
    var endDate = event.end;
    var calEvent = itemOccurrence.QueryInterface(Components.interfaces.calIEvent);

    //HACK because event.start is convert to the proper TZ, but
    //event.start.jsDate is not!
    var adjustedStartDate = new Date(startDate.year, startDate.month, startDate.day);
    var adjustedEndDate = new Date(endDate.year, endDate.month, endDate.day);

    // Check if the event is within the bounds of events to be displayed.
    if ((adjustedEndDate < this.displayStartDate) ||
        (adjustedStartDate > this.displayEndDate) ||
        (adjustedEndDate == this.displayStartDate &&
         adjustedStartDate < this.displayStartDate))
        return;

    /*
    if (calEvent.isAllDay) {
        endDate = endDate.clone();
        endDate.hour = 23;
        endDate.minute = 59;
        endDate.normalize();
    }
    */
    debug("all day:   " + calEvent.startDate.isDate + "\n");
    debug("startdate: " + startDate + "\n");
    debug("enddate:   " + endDate + "\n");

    var startHour = startDate.hour;
    var startMinutes = startDate.minute;
    var eventDuration = (endDate.jsDate - startDate.jsDate) / (60 * 60 * 1000);

    debug("duration:  " + eventDuration + "\n");

    var eventBox = document.createElement("vbox");

    // XXX Consider changing this to only store the ID
    eventBox.occurrence = itemOccurrence;
    eventBox.event = calEvent;

    // figure out what column we need to put this on
    var index = (startDate.weekday - this.displayStartDate.getDay() + 7) % 7;

    var ElementOfRef = document.getElementById("week-tree-day-" + gRefColumnIndex + "-item-" + startHour) ;
    // All-day events won't find a good ElementOfRef normally,
    if (!ElementOfRef.boxObject.width) {
        var sHour = getIntPref(this.calendarWindow.calendarPreferences.calendarPref,
                               "event.defaultstarthour", 8);
        ElementOfRef = document.getElementById("week-tree-day-" + gRefColumnIndex 
                                                + "-item-" + sHour);
    }
    var hourHeight = ElementOfRef.boxObject.height;
    var ElementOfRefEnd = document.getElementById("week-tree-day-" + gRefColumnIndex + "-item-" + endDate.hour) ;
    var hourHeightEnd = ElementOfRefEnd.boxObject.height;
    var hourWidth = ElementOfRef.boxObject.width;

    if (!startDate.isDate) {
      var eventSlotWidth = Math.round(hourWidth / event.totalSlotCount);

      var Width = ( event.drawSlotCount * eventSlotWidth ) - 1;
      eventBox.setAttribute( "width", Width );

      var top = eval( ElementOfRef.boxObject.y + ( ( startMinutes/60 ) * hourHeight ) );
      top = top - ElementOfRef.parentNode.boxObject.y - 2;
      eventBox.setAttribute("top", top);

      var bottom = eval( ElementOfRefEnd.boxObject.y + ( ( endDate.minute/60 ) * hourHeightEnd ) );
      bottom = bottom - ElementOfRefEnd.parentNode.boxObject.y - 2;
      eventBox.setAttribute("height", bottom - top);

      var boxLeft = document.getElementById("week-tree-day-"+index+"-item-"+startHour).boxObject.x - 
                    document.getElementById( "week-view-content-box" ).boxObject.x +
                    ( event.startDrawSlot * eventSlotWidth );
      //dump(boxLeft + "\n");
      eventBox.setAttribute("left", boxLeft);
    } else {
      eventBox.setAttribute( "width", hourWidth );
    }
   
    // set the event box to be of class week-view-event-class and the appropriate calendar-color class
    this.setEventboxClass(eventBox, calEvent, "week-view");
  
    eventBox.setAttribute("eventbox", "weekview");
    eventBox.setAttribute("dayindex", index + 1);
    eventBox.setAttribute("onclick", "weekEventItemClick(this, event)" );
    eventBox.setAttribute("ondblclick", "weekEventItemDoubleClick(this, event)");
    eventBox.setAttribute("id", "week-view-event-box-" + calEvent.id);
    eventBox.setAttribute("name", "week-view-event-box-" + calEvent.id);
    eventBox.setAttribute("onmouseover", "onMouseOverGridOccurrence(event)" );
    eventBox.setAttribute("tooltip", "gridOccurrenceTooltip");

    // Event box text (title, location and description)
    var locationText = calEvent.getProperty("LOCATION");
    if (calEvent.title || locationText) {
      var titleText = ( (calEvent.title || "") + 
        (locationText ? " ("+locationText+")" : "") );
      var titleElement = document.createElement( "label" );
      titleElement.setAttribute( "class", "week-view-event-title-label-class" );
      titleElement.appendChild( document.createTextNode( titleText ));
      eventBox.appendChild( titleElement );
    }
    var descriptionText = calEvent.getProperty("DESCRIPTION");
    if (descriptionText) {
      var descriptionElement = document.createElement( "description" );
      descriptionElement.setAttribute( "class", "week-view-event-description-class" );
      descriptionElement.appendChild(document.createTextNode(descriptionText));
      eventBox.appendChild( descriptionElement );
    }
    if(this.calendarWindow.EventSelection.isSelectedEvent(calEvent))
        eventBox.setAttribute( "eventselected", "true" );

    debug("Adding eventBox " + eventBox + "\n");
    if (!startDate.isDate) {
      document.getElementById("week-view-content-board").appendChild(eventBox);
    } else {
      var allDayBox = document.getElementById("all-day-content-box-week-"+(index+1));
      allDayBox.appendChild(eventBox);
    }
}

/** PUBLIC
*
*   Called when the user switches to a different view
*/
WeekView.prototype.switchFrom = function( )
{
  //Enable menu options
  document.getElementById( "only-workday-checkbox-1" ).setAttribute( "disabled", "true" );
  document.getElementById( "only-workday-checkbox-2" ).setAttribute( "disabled", "true"  );
}

/** PUBLIC
*
*   Called when the user switches to the week view
*/
WeekView.prototype.switchTo = function( )
{
   // disable/enable view switching buttons   
   var weekViewButton = document.getElementById( "week_view_command" );
   var monthViewButton = document.getElementById( "month_view_command" );
   var dayViewButton = document.getElementById( "day_view_command" );
   
   monthViewButton.removeAttribute( "disabled" );
   weekViewButton.setAttribute( "disabled", "true" );
   dayViewButton.removeAttribute( "disabled" );
   
   //Enable menu options
   document.getElementById( "only-workday-checkbox-1" ).removeAttribute( "disabled" );
   document.getElementById( "only-workday-checkbox-2" ).removeAttribute( "disabled" );
   
   // switch views in the deck
   var calendarDeckItem = document.getElementById( "calendar-deck" );
   calendarDeckItem.selectedIndex = 1;
}

/** PUBLIC
*
*   Redraw the display, but not the events
*/
WeekView.prototype.refreshDisplay = function( )
{
  var Offset = this.preferredWeekStart();
  var isOnlyWorkDays = (gOnlyWorkdayChecked == "true");
  var isDayOff = this.preferredDaysOff();
   
   // Define a reference column (which will not be collapsed latter) to use to get its width. 
   // This is used to place the event Box
  gRefColumnIndex = Offset;
  if (isOnlyWorkDays) { 
    for (var day = 0; day < 7; day++) {
      if (!isDayOff[day]) {
        gRefColumnIndex = day;
        break;
      }
    }
    isDayOff[gRefColumnIndex] = false; // in case all days off, make one visible.
   }
   
   // Set the from-to title string, based on the selected date
   var selectedDate = this.calendarWindow.getSelectedDate();
   var viewDay = selectedDate.getDay();
   var viewDayOfMonth = selectedDate.getDate();
   var viewMonth = selectedDate.getMonth();
   var viewYear = selectedDate.getFullYear();
   
   var NewArrayOfDayNames = new Array();
   
   // Set the header information for the week view
   var i;
   for( i = 0; i < ArrayOfDayNames.length; i++ ) {
      NewArrayOfDayNames[i] = ArrayOfDayNames[i];
   }
   
   for( i = 0; i < Offset; i++ ) {
      var FirstElement = NewArrayOfDayNames.shift();
      NewArrayOfDayNames.push( FirstElement );
   }
   
   viewDay -= Offset;
   if( viewDay < 0 ) {
      viewDay += 7;
   }
   var dateOfLastDayInWeek = viewDayOfMonth + ( 6 - viewDay );
   var dateOfFirstDayInWeek = viewDayOfMonth - viewDay;
   
   var firstDayOfWeek = new Date( viewYear, viewMonth, dateOfFirstDayInWeek );
   var lastDayOfWeek = new Date( viewYear, viewMonth, dateOfLastDayInWeek );
   
   var firstDayMonthName = this.calendarWindow.dateFormater.getFormatedDateWithoutYear( firstDayOfWeek );
   var lastDayMonthName =  this.calendarWindow.dateFormater.getFormatedDateWithoutYear( lastDayOfWeek );
   
   var mondayDiff = (Offset >= 5) ? 8 - Offset : 1 - Offset ;
   var mondayDate = new Date( firstDayOfWeek.getFullYear(), 
                              firstDayOfWeek.getMonth(),
                              firstDayOfWeek.getDate() + mondayDiff );
   
   var weekNumber = DateUtils.getWeekNumber(mondayDate);
   
   var weekViewStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
   var dateString = weekViewStringBundle.GetStringFromName( "Week" )+" "+weekNumber+ ": "+firstDayMonthName + " - " + lastDayMonthName ;
   
   var weekTextItem = document.getElementById( "week-title-text" );
   weekTextItem.setAttribute( "value" , dateString ); 
   /* done setting the header information */
   
   /* Fix the day names because users can choose which day the week starts on  */
   for( var dayIndex = 1; dayIndex <= 7; ++dayIndex ) {
      var dateOfDay = firstDayOfWeek.getDate();
      
      var headerDateItem = document.getElementById( "week-header-date-"+dayIndex );
      headerDateItem.setAttribute( "value" , dateOfDay );
      headerDateItem.setAttribute( "date", firstDayOfWeek );
      headerDateItem.numEvents = 0;
      
     var col = dayIndex - 1;

      document.getElementById( "week-header-date-text-"+dayIndex ).setAttribute( "value", NewArrayOfDayNames[col] );
         
      var headColumn = document.getElementById( "weekview-header-column-day-"+dayIndex );
      var bodyColumn = document.getElementById( "weekview-column-day-"+dayIndex );
      var dayOfWeek = (Offset + col) % 7;
      if( isOnlyWorkDays && isDayOff[ dayOfWeek ]) {
        headColumn.setAttribute( "hidden", "true" );
        bodyColumn.setAttribute( "hidden", "true" );
      } else {
        headColumn.removeAttribute( "hidden" );
        bodyColumn.removeAttribute( "hidden" );
        if ( isDayOff[ dayOfWeek ] ) {
          headColumn.setAttribute("weekend", "true");
          bodyColumn.setAttribute("weekend", "true");
        } else {
          headColumn.removeAttribute("weekend");
          bodyColumn.removeAttribute("weekend");
        }
      }
      // advance to next day 
      firstDayOfWeek.setDate( dateOfDay + 1 );
   }
   this.hiliteTodaysDate( );
}

/** PUBLIC
*
*   Go to the next week.
*/
WeekView.prototype.goToNext = function()
{
   var nextWeek = new Date( this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(),  this.calendarWindow.selectedDate.getDate() + 7 );
   this.goToDay( nextWeek );
}

/** PUBLIC
*
*   Go to the previous week.
*/
WeekView.prototype.goToPrevious = function()
{
   var prevWeek = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(),  this.calendarWindow.selectedDate.getDate() - 7 );
   this.goToDay( prevWeek );
}

WeekView.prototype.selectBoxForEvent = function( calendarEvent )
{
   var EventBoxes = document.getElementsByAttribute( "name", "week-view-event-box-"+calendarEvent.id );
   for ( var j = 0; j < EventBoxes.length; j++ ) {
      EventBoxes[j].setAttribute( "eventselected", "true" );
   }
}

WeekView.prototype.getVisibleEvent = function( calendarEvent )
{
   var eventBox = document.getElementById( "week-view-event-box-"+calendarEvent.id );
   if ( eventBox ) {
      return eventBox;
   } else {
      return false;
   }
}


/** PRIVATE
*
*   Mark the selected date, also unmark the old selection if there was one
*/
WeekView.prototype.hiliteSelectedDate = function multiweekView_hiliteSelectedDate( )
{
   // Clear the old selection if there was one
   
   this.clearSelectedDate();
   this.clearSelectedEvent();
   
   // get the events for the day and loop through them
   var FirstDay = new Date( gHeaderDateItemArray[1].getAttribute( "date" ) );
   var LastDay  = new Date( gHeaderDateItemArray[7].getAttribute( "date" ) );
   LastDay.setHours( 23 );
   LastDay.setMinutes( 59 );
   LastDay.setSeconds( 59 );
   var selectedDay = this.calendarWindow.getSelectedDate();
   
   if ( selectedDay.getTime() > FirstDay.getTime() && selectedDay.getTime() < LastDay.getTime() ) {
      var ThisDate;
      //today is visible, get the day index for today
      for ( var dayIndex = 1; dayIndex <= 7; ++dayIndex ) {
         ThisDate = new Date( gHeaderDateItemArray[dayIndex].getAttribute( "date" ) );
         if ( ThisDate.getFullYear() == selectedDay.getFullYear()
                && ThisDate.getMonth() == selectedDay.getMonth()
                && ThisDate.getDay()   == selectedDay.getDay() ) {
            break;
         }
      }
      //dayIndex now contains the box numbers that we need.
      for (var i = 0; i < 24; i++ ) {
         document.getElementById( "week-tree-day-"+(dayIndex-1)+"-item-"+i ).setAttribute( "weekselected", "true" );
      }
   }
}

/** PRIVATE
*
*  Mark today as selected, also unmark the old today if there was one.
*/
WeekView.prototype.hiliteTodaysDate = function( )
{
   //clear out the old today boxes.
  this.removeAttributeFromElements("today","true");
   
   // get the events for the day and loop through them
   var FirstDay = new Date( gHeaderDateItemArray[1].getAttribute( "date" ) );
   var LastDay  = new Date( gHeaderDateItemArray[7].getAttribute( "date" ) );
   LastDay.setHours( 23 );
   LastDay.setMinutes( 59 );
   LastDay.setSeconds( 59 );
   var Today = new Date();
   
   if ( Today.getTime() > FirstDay.getTime() && Today.getTime() < LastDay.getTime() ) {
      var ThisDate;
      //today is visible, get the day index for today
      for ( var dayIndex = 1; dayIndex <= 7; ++dayIndex ) {
         ThisDate = new Date( gHeaderDateItemArray[dayIndex].getAttribute( "date" ) );
         if ( ThisDate.getFullYear() == Today.getFullYear() &&
              ThisDate.getMonth()    == Today.getMonth() &&
              ThisDate.getDay()      == Today.getDay() ) {
            break;
         }
      }
      //dayIndex now contains the box numbers that we need.
      for (var i = 0; i < 24; i++ ) {
         document.getElementById( "week-tree-day-"+(dayIndex-1)+"-item-"+i ).setAttribute( "today", "true" );
      }
   }
}

/** PUBLIC
*
*   clear the selected event by taking off the selected attribute.
*/
WeekView.prototype.clearSelectedEvent = function( )
{
   //Event = gCalendarWindow.getSelectedEvent();
  this.removeAttributeFromElements("eventselected", "true");
}

/** PUBLIC
*
*   This is called when we are about the make a new event
*   and we want to know what the default start date should be for the event.
*/
WeekView.prototype.getNewEventDate = function( )
{
   var start = new Date( this.calendarWindow.getSelectedDate() );
   start.setHours( start.getHours() );
   start.setMinutes( Math.ceil( start.getMinutes() / 5 ) * 5 );
   start.setSeconds( 0 );
   
   return start; 
}

/** PUBLIC
*
*  Unmark the selected date if there is one.
*/
WeekView.prototype.clearSelectedDate = function( )
{
  this.removeAttributeFromElements("weekselected","true");
}
