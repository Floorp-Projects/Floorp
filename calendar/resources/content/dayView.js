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
 *                 Karl Guertin <grayrest@grayrest.com>
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
*   DayView Class  subclass of CalendarView
*
*  Calendar day view class
*
* PROPERTIES
*
*    NOTES
*
* 
*/

// Make DayView inherit from CalendarView

DayView.prototype = new CalendarView();
DayView.prototype.constructor = DayView;

/**
*   DayView Constructor.
* 
* PARAMETERS
*      calendarWindow     - the owning instance of CalendarWindow.
*
*/


function DayView( calendarWindow )
{
   // call super
   
   this.superConstructor( calendarWindow );
   
   var TimeToFormat = new Date();
   TimeToFormat.setMinutes( "0" );
   
   //set the time on the left hand side labels
   //need to do this in JavaScript to preserve formatting
   for( var i = 0; i < 24; i++ )
   {
      /*
      ** FOR SOME REASON, THIS IS NOT WORKING FOR MIDNIGHT
      */ 
      TimeToFormat.setHours( i );

      var FormattedTime = calendarWindow.dateFormater.getFormatedTime( TimeToFormat );

      var Label = document.getElementById( "day-view-hour-"+i );
      Label.setAttribute( "value", FormattedTime );
   }
   
   var dayViewEventSelectionObserver =
   {
      onSelectionChanged : function dv_EventSelectionObserver_OnSelectionChanged( EventSelectionArray )
      {
         for( i = 0; i < EventSelectionArray.length; i++ )
         {
            gCalendarWindow.dayView.selectBoxForEvent( EventSelectionArray[i] );
         }
      }
   }
      
   calendarWindow.EventSelection.addObserver( dayViewEventSelectionObserver );
}

/** PUBLIC
*
*   Redraw the events for the current day
*/

DayView.prototype.refreshEvents = function dayview_refreshEvents( ) {
   this.kungFooDeathGripOnEventBoxes = new Array();
   
   var dayEventList = gEventSource.getEventsForDay( this.calendarWindow.getSelectedDate() );
   var allDayEvents = new Array();
   var normalEvents = new Array();
   var calendarStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
   
   // DOM elements
   var collapsibleBox = document.getElementById( "all-day-collapsible-box" );
   var allDayBox = document.getElementById( "all-day-content-box" );
   var allDayLabel = document.getElementById( "all-day-content-title" );
   var dayViewContent = document.getElementById( "day-view-content-box" );
   
   //remove all the all day row -boxes from the all day content box.
   while( allDayBox.hasChildNodes() )
      allDayBox.removeChild( allDayBox.firstChild );
   
   this.removeElementsByAttribute("eventbox", "dayview");
   
   // set view limits for the day
   var limits = this.getViewLimits(dayEventList,this.calendarWindow.getSelectedDate());
   var i;
   for( i = 0; i < 24; i++ ) {
      if( ( i < limits.startHour ) || ( i > limits.endHour ) )
         document.getElementById( "day-tree-item-"+i ).setAttribute( "collapsed", "true" );
      else
         document.getElementById( "day-tree-item-"+i ).removeAttribute( "collapsed" );
   }
   
   // Divide events into all day and other events
   for( i = 0; i < dayEventList.length; i++ ) {
      if ( dayEventList[i].event.allDay == true )
         allDayEvents.push(dayEventList[i]);
      else
         normalEvents.push(dayEventList[i]);
   }
   
   // Calculate event draw properties (where events are drawn )
   this.setDrawProperties(normalEvents);
   this.setAllDayDrawProperties(allDayEvents, this.calendarWindow.getSelectedDate() );
   
   // Sort all day events to correct draw order
   allDayEvents.sort(this.compareAllDayEvents);
   
   // add allday events to DOM (if any)
   if( allDayEvents.length == 0 ) {
      dayViewContent.removeAttribute( "allday" );
      collapsibleBox.setAttribute( "collapsed", "true" );
   } else {
      //resize the day's content box.
      dayViewContent.setAttribute( "allday", "true" );
      //show the all day box
      collapsibleBox.removeAttribute( "collapsed" );
      
      allDayLabel.value = calendarStringBundle.GetStringFromName( "AllDayEvents" );
      allDayLabel.setAttribute("width", kDayViewHourLeftStart - 10);
      
      for( i = 0; i < allDayEvents.length; i++ ) {
         eventBox = this.createAllDayEventBox( allDayEvents[i] );
         this.insertAllDayEventBox(eventBox, allDayBox);
         
         this.kungFooDeathGripOnEventBoxes.push( eventBox );
      }
   }
   
   // Add non-allday events to DOM
   for ( i = 0; i < normalEvents.length; ++i )
   {
      eventBox = this.createEventBox( normalEvents[i] );
      document.getElementById( "day-view-content-board" ).appendChild( eventBox );
      this.kungFooDeathGripOnEventBoxes.push( eventBox );
   }
}

/** PRIVATE
*
*   Inserts the eventBox into DOM, possibly creating a new all day row (hbox) as well
*    This could probably be done more efficiently. Curently the eventbox
*    is inserted in to a hbox. Then if the width of the hbox has changed
*    (hbox is wider than the page), the eventbox is removed from the hbox,
*    and the next hbox is tried...
*/
DayView.prototype.insertAllDayEventBox = function dayview_insertAllDayEventBox( eventBox, allDayBox ) {
   var allDayRow;
   var inserted = false;
   var lastOnRow;
   var rowWidth;
   
   if( !eventBox.calendarEventDisplay.allDayStartsBefore ) {
      var rows = allDayBox.childNodes;
      for( var row = 0; row < rows.length; row++) {
         lastOnRow = rows[row].lastChild;
         rowWidth = rows[row].boxObject.width;

         if( lastOnRow.calendarEventDisplay.allDayEndsAfter ) {
            if( !eventBox.calendarEventDisplay.allDayEndsAfter && 
                !lastOnRow.calendarEventDisplay.allDayStartsBefore) {
               rows[row].insertBefore(eventBox,rows[row].lastChild);
               inserted = true;
            }
         } else {
            rows[row].appendChild(eventBox);
            inserted = true;
         } 

         if( inserted ) {
            // test if row really had room for this event...
            if( rows[row].boxObject.width > rowWidth ) {
               // row is too full
               rows[row].removeChild(eventBox);
               inserted = false;
            } else {
               break;
            }
         }
      }
   }
   if( !inserted ) {
      // allDayStartsBefore == true or a proper place was not found.
      // Must add a row to allDayBox
      allDayRow = document.createElement( "hbox" );
      allDayRow.appendChild( eventBox );
      allDayBox.appendChild( allDayRow );
   }
}

/** PRIVATE
*
* Compare function for array.sort(). Events will be sorted into a proper oder
* for day view all-day event drawing.
*/
DayView.prototype.compareAllDayEvents = function dayview_compareAllDayEvents( a, b ) {
   if( ( ( a.allDayStartsBefore )  && ( a.allDayEndsAfter )  ) &&
       ( ( !b.allDayStartsBefore ) || ( !b.allDayEndsAfter ) ) )  {
      return -1;
   } else if( ( ( b.allDayStartsBefore )  && ( b.allDayEndsAfter )  ) &&
              ( ( !a.allDayStartsBefore ) || ( !a.allDayEndsAfter ) ) ) {
      return 1;
   } else {
   
      if( a.allDayStartsBefore ) {
         if( !b.allDayStartsBefore )
            return -1;
      } else {
         if( b.allDayStartsBefore ) 
            return 1;
      }
      
      var res = a.event.start.getTime() - b.event.start.getTime();
      if( res == 0)
         res = a.event.end.getTime() - b.event.end.getTime();
      return res;
   }
}

/** PRIVATE
*
*   This creates an all day event box for the day view
*/
DayView.prototype.createAllDayEventBox = function dayview_createAllDayEventBox( calendarEventDisplay ) {
   var containerName = this.calendarWindow.calendarManager.getCalendarByName(
                         calendarEventDisplay.event.parent.server ).subject.split(":")[2];

   // build up the label and image for the eventbox
   var eventText = calendarEventDisplay.event.title;
   var locationText = calendarEventDisplay.event.location;
   var newLabel = document.createElement( "label" );
   newLabel.setAttribute( "class", "day-view-allday-event-title-label-class" );
   if( calendarEventDisplay.event.location )
      newLabel.setAttribute( "value", eventText+" (" + locationText + ")" );
   else 
      newLabel.setAttribute( "value", eventText );

   var newImage = document.createElement("image");
   newImage.setAttribute( "class", "all-day-event-class" );
   
   //create the actual event box
   var eventBox = document.createElement( "hbox" );
   eventBox.calendarEventDisplay = calendarEventDisplay;
   eventBox.appendChild( newImage );
   eventBox.appendChild( newLabel );
   eventBox.setAttribute( "name", "day-view-event-box-" + calendarEventDisplay.event.id );
   
   // set calendar name as attribute to enable calendar specific colors
   eventBox.setAttribute( "class", "day-view-all-day-event-class " + containerName);
   
   eventBox.setAttribute( "onclick", "dayEventItemClick( this, event )" );
   eventBox.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
   eventBox.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
   eventBox.setAttribute( "tooltip", "eventTooltip" );
   
   eventBox.setAttribute( "flex", "1" );  
   
   // mark the box as selected, if the event is
   if( this.calendarWindow.EventSelection.isSelectedEvent( calendarEventDisplay.event ) )
      eventBox.setAttribute( "eventselected", "true" );
   
   if( calendarEventDisplay.allDayStartsBefore )
      eventBox.setAttribute("continues-left", "true");
   if( calendarEventDisplay.allDayEndsAfter)
      eventBox.setAttribute("continues-right", "true");
   
   return eventBox;
}

/** PRIVATE
*
*   This creates an event box for the day view
*/
DayView.prototype.createEventBox = function dayview_createEventBox( calendarEventDisplay ) {
   //if you change this class, you have to change calendarViewDNDObserver in calendarDragDrop.js
   var containerName = this.calendarWindow.calendarManager.getCalendarByName(
                         calendarEventDisplay.event.parent.server ).subject.split(":")[2];
   
   var displayDateObject = new Date( calendarEventDisplay.displayDate );
   var startHour = displayDateObject.getHours();
   var startMinutes = displayDateObject.getMinutes();
   var eventDurationHours = ( ( calendarEventDisplay.displayEndDate 
                                - calendarEventDisplay.displayDate ) 
                              / ( kDate_MillisecondsInHour ) );
   
   var startHourTreeItem = document.getElementById( "day-tree-item-"+startHour );
   
   var hourHeight = startHourTreeItem.boxObject.height;
   var hourWidth = startHourTreeItem.boxObject.width;
   var eventSlotWidth = Math.round( ( hourWidth - kDayViewHourLeftStart ) 
                                    / calendarEventDisplay.totalSlotCount );

   var eventLocation = calendarEventDisplay.event.location;

   //calculate event dimensions
   var eventTop = startHourTreeItem.boxObject.y -
                  startHourTreeItem.parentNode.boxObject.y +
                  Math.round( hourHeight * startMinutes/ 60 ) - 1;
   var eventLeft = kDayViewHourLeftStart + ( calendarEventDisplay.startDrawSlot * eventSlotWidth );
   var eventHeight = Math.round( eventDurationHours * hourHeight ) + 1;
   var eventWidth = ( calendarEventDisplay.drawSlotCount * eventSlotWidth ) - 1;
   
   // create title label, location label and description description :)
   var eventTitleLabel = document.createElement( "label" );
   eventTitleLabel.setAttribute( "class", "day-view-event-title-label-class" );
   eventTitleLabel.setAttribute( "crop", "end" );
   eventTitleLabel.setAttribute( "flex", "1" );
   if( eventLocation ) 
      eventTitleLabel.setAttribute( "value", calendarEventDisplay.event.title + " (" + eventLocation + ")" );
   else
      eventTitleLabel.setAttribute( "value", calendarEventDisplay.event.title );

   var eventText = document.createTextNode( calendarEventDisplay.event.description );
   var eventDescription = document.createElement( "description" );
   eventDescription.setAttribute( "class", "day-view-event-description-class" );
   eventDescription.appendChild( eventText );
   
   //create actual eventbox
   var eventBox = document.createElement( "vbox" );
   eventBox.calendarEventDisplay = calendarEventDisplay;
   eventBox.setAttribute( "name", "day-view-event-box-"+calendarEventDisplay.event.id );
   // set calendar name as attribute to enable calendar specific colors
   eventBox.setAttribute("class", "day-view-event-class " + containerName );
   eventBox.setAttribute( "top", eventTop );
   eventBox.setAttribute( "left", eventLeft );
   eventBox.setAttribute( "height", eventHeight );
   eventBox.setAttribute( "width", eventWidth );
   
   eventBox.setAttribute( "flex", "1" );
   eventBox.setAttribute( "eventbox", "dayview" ); // ?
   
   eventBox.setAttribute( "onclick", "dayEventItemClick( this, event )" );
   eventBox.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
   eventBox.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
   eventBox.setAttribute( "tooltip", "eventTooltip" );
   eventBox.setAttribute( "ondraggesture", "nsDragAndDrop.startDrag(event,calendarViewDNDObserver);" );
   eventBox.setAttribute( "ondragover", "nsDragAndDrop.dragOver(event,calendarViewDNDObserver)" );
   eventBox.setAttribute( "ondragdrop", "nsDragAndDrop.drop(event,calendarViewDNDObserver)" );
   
   // mark the box as selected, if the event is
   if( this.calendarWindow.EventSelection.isSelectedEvent( calendarEventDisplay.event ) )
      eventBox.setAttribute( "eventselected", "true" );
   
   eventBox.appendChild( eventTitleLabel );
   eventBox.appendChild( eventDescription );   
   return eventBox;
}

/** PUBLIC
*
*   Called when the user switches from a different view
*/

DayView.prototype.switchFrom = function dayview_switchFrom( )
{
}


/** PUBLIC
*
*   Called when the user switches to the day view
*/

DayView.prototype.switchTo = function dayview_switchTo( )
{
   // disable/enable view switching buttons   

   var weekViewButton = document.getElementById( "week_view_command" );
   var monthViewButton = document.getElementById( "month_view_command" );
   var dayViewButton = document.getElementById( "day_view_command" );
   
   monthViewButton.removeAttribute( "disabled" );
   weekViewButton.removeAttribute( "disabled" );
   dayViewButton.setAttribute( "disabled", "true" );
   
   // switch views in the deck
   
   var calendarDeckItem = document.getElementById( "calendar-deck" );
   calendarDeckItem.selectedIndex = 2;
}


/** PUBLIC
*
*   Redraw the display, but not the events
*/

DayView.prototype.refreshDisplay = function dayview_refreshDisplay( )
{
   // update the title
   var dayNamePrev1;
   var dayNamePrev2;

   var dayName = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() );
	if (this.calendarWindow.getSelectedDate().getDay() < 2)
	{
      if (this.calendarWindow.getSelectedDate().getDay() == 0)
		{
			dayNamePrev1 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() + 6 );
			dayNamePrev2 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() + 5 );
		}
		else
		{
			dayNamePrev1 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() - 1 );
			dayNamePrev2 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() + 5 );
		}
	}
	else
	{
		dayNamePrev1 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() - 1 );
		dayNamePrev2 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() - 2 );
	}
	
   var dayNameNext1;
   var dayNameNext2;
   
   if (this.calendarWindow.getSelectedDate().getDay() > 4)
	{
		if (this.calendarWindow.getSelectedDate().getDay() == 6)
		{
			dayNameNext1 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() - 6);
			dayNameNext2 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() - 5);
		}
		else
		{
			dayNameNext1 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() + 1);
			dayNameNext2 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() - 5);
		}
	}
	else
	{
		dayNameNext1 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() + 1);
		dayNameNext2 = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() + 2);
	}
   
   var weekNumber = DateUtils.getWeekNumber( this.calendarWindow.getSelectedDate() );

   var weekViewStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");

   var dateString = weekViewStringBundle.GetStringFromName( "Week" )+" "+weekNumber+ ": " + this.calendarWindow.dateFormater.getFormatedDate( this.calendarWindow.getSelectedDate() ) ;
   
   var dayTextItemPrev2 = document.getElementById( "d-2-day-title" );
   var dayTextItemPrev1 = document.getElementById( "d-1-day-title" );
   var dayTextItem = document.getElementById( "d0-day-title" );
   var dayTextItemNext1 = document.getElementById( "d1-day-title" );
   var dayTextItemNext2 = document.getElementById( "d2-day-title" );
	var daySpecificTextItem = document.getElementById( "d0-day-specific-title" );
   dayTextItemPrev2.setAttribute( "value" , dayNamePrev2 );
   dayTextItemPrev1.setAttribute( "value" , dayNamePrev1 );
   dayTextItem.setAttribute( "value" , dayName );
   dayTextItemNext1.setAttribute( "value" , dayNameNext1 );
   dayTextItemNext2.setAttribute( "value" , dayNameNext2 );
	daySpecificTextItem.setAttribute( "value" , dateString );
}


/** PUBLIC
*
*   This is called when we are about the make a new event
*   and we want to know what the default start date should be for the event.
*/

DayView.prototype.getNewEventDate = function dayview_getNewEventDate( )
{
   var start = new Date( this.calendarWindow.getSelectedDate() );
   
   start.setHours( start.getHours() );
   start.setMinutes( Math.ceil( start.getMinutes() / 5 ) * 5 );
   start.setSeconds( 0 );
   
   return start; 
}


/** PUBLIC
*
*   Go to the next day.
*/

DayView.prototype.goToNext = function dayview_goToNext(goDays)
{
   var nextDay;

   if (goDays)
	{
		nextDay = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(), this.calendarWindow.selectedDate.getDate() + goDays );
      this.goToDay( nextDay );
   }
	else
	{
      nextDay = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(), this.calendarWindow.selectedDate.getDate() + 1 );
      this.goToDay( nextDay );
   }
}


/** PUBLIC
*
*   Go to the previous day.
*/

DayView.prototype.goToPrevious = function dayview_goToPrevious( goDays )
{
   var prevDay;

   if (goDays)
	{
      prevDay = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(), this.calendarWindow.selectedDate.getDate() - goDays );
      this.goToDay( prevDay );
   }
	else
	{
      prevDay = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(), this.calendarWindow.selectedDate.getDate() - 1 );
      this.goToDay( prevDay );
   }
}


DayView.prototype.selectBoxForEvent = function dayview_selectBoxForEvent( calendarEvent )
{
   var EventBoxes = document.getElementsByAttribute( "name", "day-view-event-box-"+calendarEvent.id );
   for ( var j = 0; j < EventBoxes.length; j++ ) 
   {
      EventBoxes[j].setAttribute( "eventselected", "true" );
   }
}

/** PUBLIC
*
*   clear the selected event by taking off the selected attribute.
*/
DayView.prototype.clearSelectedEvent = function dayview_clearSelectedEvent( )
{
  debug("clearSelectedEvent");
  this.removeAttributeFromElements("eventselected", "true");
}


DayView.prototype.clearSelectedDate = function dayview_clearSelectedDate( )
{
   return;
}


DayView.prototype.getVisibleEvent = function dayview_getVisibleEvent( calendarEvent )
{
   eventBox = document.getElementById( "day-view-event-box-"+calendarEvent.id );
   if ( eventBox ) 
   {
      return eventBox;
   }
   else
      return false;

}

/*
** This function is needed because it may be called after the end of each day.
*/

DayView.prototype.hiliteTodaysDate = function dayview_hiliteTodaysDate( )
{
   return;
}


function debug( Text )
{
   dump( "\ndayView.js: "+ Text);
}

