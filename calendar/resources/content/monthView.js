/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley (garths@oeone.com) , Mike Potter (mikep@oeone.com)
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
 * 
*/

/*-----------------------------------------------------------------
*   MonthView Class  subclass of CalendarView
*
*  Calendar month view class
*
* PROPERTIES
*     selectedEventBox     - Events are displayed in dynamically created event boxes
*                            this is the selected box, or null
*   
*     showingLastDay       - When the user changes to a new month we select
*                            the same day in the new month that was selected in the original month. If the
*                            new month does not have that day ( i.e. 31 was selected and the new month has 
*                            only 30 days ) we move the selection to the last day. When this happens we turn on
*                            the 'showingLastDay' flag. Now we will always select the last day when the month
*                            is changed so that if they go back to the original month, 31 is selected again.
*                            'showingLastDay' is turned off when the user selects a new day or changes the view.
*                       
*                            
*     dayNumberItemArray   - An array [ 0 to 41 ] of text boxes that hold the day numbers in the month view.
*                            We set the value attribute to the day number, or "" for boxes that are not in the month.
*                            In the XUL they have id's of the form  month-week-<row_number>-day-<column_number>
*                            where row_number is 1 - 6 and column_number is 1 - 7.
*
*     dayBoxItemArray      - An array [ 0 to 41 ] of  boxes, one for each day in the month view. These boxes
*                            are selected when a day is selected. They contain a dayNumberItem and event boxes.
*                            In the XUL they have id's of the form  month-week-<row_number>-day-<column_number>-box
*                            where row_number is 1 - 6 and column_number is 1 - 7.
*
*    dayBoxItemByDateArray - This array is reconstructed whenever the month changes ( and probably more
*                            often than that ) It contains day box items, just like the dayBoxItemArray above,
*                            except this array contains only those boxes that belong to the current month
*                            and is indexed by date. So for a 30 day month that starts on a Wednesday, 
*                            dayBoxItemByDateArray[0]  === dayBoxItemArray[3] and 
*                            dayBoxItemByDateArray[29] === dayBoxItemArray[36]
*
*    kungFooDeathGripOnEventBoxes - This is to keep the event box javascript objects around so when we get 
*                                   them back they still have the calendar event property on them.
* 
*
*    NOTES
*
*       Events are displayed in dynamically created event boxes. these boxes have a property added to them
*       called "calendarEvent" which contains the event represented by the box. 
*
*       There is one day box item for every day box in the month grid. These have an attribute
*       called "empty" which is set to "true", like so: 
*
*                                   dayBoxItem.setAttribute( "empty" , "true" );
*
*       when the day box is not in the month. This allows the display to be controlled from css.
*
*       The day boxes also have a couple of properties added to them:
*
*            dayBoxItem.dayNumber  - null when day is not in month.
*                                  - the date, 1 to 31, otherwise,
*           
*            dayBoxItem.numEvents  - The number of events for the day, used to limit the number displayed
*                                    since there is only room for 3.
* 
*/

// Make MonthView inherit from CalendarView

MonthView.prototype = new CalendarView();
MonthView.prototype.constructor = MonthView;


/**
*   MonthView Constructor.
* 
* PARAMETERS
*      calendarWindow     - the owning instance of CalendarWindow.
*
*/

function MonthView( calendarWindow )
{
   // call the super constructor
   
   this.superConstructor( calendarWindow );
   
   // set up, see notes above
   
   this.selectedEventBoxes = new Array();
   
   this.selectedTodayBox = null;
   
   this.showingLastDay = false;
  
   // set up month day box's and day number text items, see notes above
   
   this.dayNumberItemArray = new Array();
   this.dayBoxItemArray = new Array();
   
   var dayItemIndex = 0;
   
   for( var weekIndex = 1; weekIndex <= 6; ++weekIndex )
   {
      for( var dayIndex = 1; dayIndex <= 7; ++dayIndex )
      {
         // add the day text item to an array[0..41]
         
         var dayNumberItem = document.getElementById( "month-week-" + weekIndex + "-day-" + dayIndex );
         this.dayNumberItemArray[ dayItemIndex ] = dayNumberItem;
         
         // add the day box to an array[0..41]
         
         var dayBoxItem = document.getElementById( "month-week-" + weekIndex + "-day-" + dayIndex + "-box" );
         this.dayBoxItemArray[ dayItemIndex ] = dayBoxItem;
         
         // set on click of day boxes
         
         dayBoxItem.setAttribute( "onclick", "gCalendarWindow.monthView.clickDay( this )" );
         
         //set the double click of day boxes
         dayBoxItem.setAttribute( "ondblclick", "gCalendarWindow.monthView.doubleClickDay( this )" );

         // array index
         
         ++dayItemIndex;
      }
   }
}

/** PUBLIC
*
*   Redraw the events for the current month
* 
*   We create XUL boxes dynamically and insert them into the XUL. 
*   To refresh the display we remove all the old boxes and make new ones.
*/
MonthView.prototype.refreshEvents = function( )
{
  // get this month's events and display them
  
   var monthEventList = this.calendarWindow.eventSource.getEventsForMonth( this.calendarWindow.getSelectedDate() );

   // remove old event boxes
   
   var eventBoxList = document.getElementsByAttribute( "eventbox", "monthview" );

   for( var eventBoxIndex = 0;  eventBoxIndex < eventBoxList.length; ++eventBoxIndex )
   {
		var eventBox = eventBoxList[ eventBoxIndex ];
      
      eventBox.parentNode.removeChild( eventBox );

   }
   
   // clear calendarEvent counts, we only display 3 events per day 
   // so to count them by adding a property to the dayItem, which is zeroed here
      
   for( var dayItemIndex = 0; dayItemIndex < this.dayBoxItemArray.length; ++dayItemIndex )
   {
      var dayItem = this.dayBoxItemArray[ dayItemIndex ];
      
      dayItem.numEvents = 0;
   }  
   
   // remove the old selection
   
   this.selectedEventBoxes = Array();
   
   // add each calendarEvent
   
   this.kungFooDeathGripOnEventBoxes = new Array();
   
   for( var eventIndex = 0; eventIndex < monthEventList.length; ++eventIndex )
   {
      var calendarEvent = monthEventList[ eventIndex ];
      
      var eventDate = calendarEvent.displayDate;
      // get the day box for the calendarEvent's day
      var eventDayInMonth = eventDate.getDate();
      
      var dayBoxItem = this.dayBoxItemByDateArray[ eventDayInMonth ];
            
      // Display no more than three, show dots for the events > 3
      
      dayBoxItem.numEvents +=  1;
      
      if( dayBoxItem.numEvents < 4 )
      {
         // Make a text item to show the event title
         
         var eventBoxText = document.createElement( "text" );
         eventBoxText.setAttribute( "crop", "right" );
         eventBoxText.setAttribute( "class", "month-day-event-text-class" );
         eventBoxText.setAttribute( "value", calendarEvent.title );
         
         // Make a box item to hold the text item
         
         var eventBox = document.createElement( "vbox" );
         eventBox.setAttribute( "id", "month-view-event-box-"+calendarEvent.id );
         eventBox.setAttribute( "name", "month-view-event-box-"+calendarEvent.id );
         eventBox.setAttribute( "event"+calendarEvent.id, true );
         eventBox.setAttribute( "class", "month-day-event-box-class" );
         eventBox.setAttribute( "eventbox", "monthview" );
         eventBox.setAttribute( "onclick", "monthEventBoxClickEvent( this, event )" );
         eventBox.setAttribute( "ondblclick", "monthEventBoxDoubleClickEvent( this, event )" );
         
         // add a property to the event box that holds the calendarEvent that the
         // box represents

         eventBox.calendarEvent = calendarEvent;
         
         this.kungFooDeathGripOnEventBoxes.push( eventBox );
         
         eventBox.setAttribute( "onmouseover", "gCalendarWindow.mouseOverInfo( calendarEvent, event )" );
         
         eventBox.setAttribute( "tooltip", "savetip" );
         
         // add the text to the event box and the event box to the day box
         
         eventBox.appendChild( eventBoxText );        
         dayBoxItem.appendChild( eventBox );
      }
      else
      {
         //if there is not a box to hold the little dots for this day...
         if ( !document.getElementById( "dotboxholder"+calendarEvent.start.getDate() ) )
         {
            //make one
            dotBoxHolder = document.createElement( "hbox" );
            
            dotBoxHolder.setAttribute( "id", "dotboxholder"+calendarEvent.start.getDate() );
            
            dotBoxHolder.setAttribute( "eventbox", "monthview" );
                        
            //add the box to the day.
            dayBoxItem.appendChild( dotBoxHolder );
         }
         else
         {
            //otherwise, get the box
            
            dotBoxHolder = document.getElementById( "dotboxholder"+calendarEvent.start.getDate() );

         }
         
         if( dotBoxHolder.childNodes.length < kMAX_NUMBER_OF_DOTS_IN_MONTH_VIEW )
         {
            eventDotBox = document.createElement( "box" );
            eventDotBox.setAttribute( "eventbox", "monthview" );
            
            //show a dot representing an event.
            
            //NOTE: This variable is named eventBox because it needs the same name as 
            // the regular boxes, for the next part of the function!
            
            eventBox = document.createElement( "image" );
            
            eventBox.setAttribute( "class", "month-view-event-dot-class" );
            
            eventBox.setAttribute( "id", "month-view-event-box-"+calendarEvent.id );
            eventBox.setAttribute( "name", "month-view-event-box-"+calendarEvent.id );
            
            eventBox.calendarEvent = calendarEvent;
            
            this.kungFooDeathGripOnEventBoxes.push( eventBox );
            
            eventBox.setAttribute( "onmouseover", "gCalendarWindow.mouseOverInfo( calendarEvent, event )" );
            eventBox.setAttribute( "onclick", "monthEventBoxClickEvent( this, event )" );
            eventBox.setAttribute( "ondblclick", "monthEventBoxDoubleClickEvent( this, event )" );
   
            eventBox.setAttribute( "tooltip", "savetip" );
   
            //add the dot to the extra box.
            eventDotBox.appendChild( eventBox );
            dotBoxHolder.appendChild( eventDotBox );
         }
         
         
      }

      // mark the box as selected, if the event is
         
      if( this.calendarWindow.getSelectedEvent() === calendarEvent )
      {
         var eventBox = gCalendarWindow.currentView.getVisibleEvent( calendarEvent );
   
         gCalendarWindow.currentView.clickEventBox( eventBox ); 

         selectEventInUnifinder( calendarEvent );
      } 
   }
}


/** PUBLIC
*
*   Called when the user switches to a different view
*/

MonthView.prototype.switchFrom = function( )
{
   this.selectedEventBoxes = Array();
}


/** PUBLIC
*
*   Called when the user switches to the month view
*/

MonthView.prototype.switchTo = function( )
{
   // see showingLastDay notes above
   
   this.showingLastDay = false;
      
   // disable/enable view switching buttons   
   
   var weekViewButton = document.getElementById( "calendar-week-view-button" );
   var monthViewButton = document.getElementById( "calendar-month-view-button" );
   var dayViewButton = document.getElementById( "calendar-day-view-button" );
   
   monthViewButton.setAttribute( "disabled", "true" );
   weekViewButton.setAttribute( "disabled", "false" );
   dayViewButton.setAttribute( "disabled", "false" );

   // switch views in the deck
   
   var calendarDeckItem = document.getElementById( "calendar-deck" );
   calendarDeckItem.setAttribute( "index", 0 );
}


/** PUBLIC
*
*   Redraw the display, but not the events
*/

MonthView.prototype.refreshDisplay = function( ShowEvent )
{ 
   // set the month/year in the header
   
   var newMonth = this.calendarWindow.getSelectedDate().getMonth();
   var newYear = this.calendarWindow.getSelectedDate().getFullYear();

   var monthName = this.calendarWindow.dateFormater.getMonthName( newMonth );
   
   monthTextItem = document.getElementById( "month-title-text" );
   
   monthTextItem.setAttribute( "value" , monthName + " " + newYear );  
   
   // Write in all the day numbers and create the dayBoxItemByDateArray, see notes above
   
   // figure out first and last days of the month
   
   var firstDate = new Date( newYear, newMonth, 1 );
   var firstDayOfWeek = firstDate.getDay();
   
   var lastDayOfMonth = DateUtils.getLastDayOfMonth( newYear, newMonth );
   
   // prepare the dayBoxItemByDateArray, we will be filling this in
   
   this.dayBoxItemByDateArray = new Array();
   
   // loop through all the day boxes
   
   var dayNumber = 1;
   
   for( var dayIndex = 0; dayIndex < this.dayNumberItemArray.length; ++dayIndex )
   {
      var dayNumberItem = this.dayNumberItemArray[ dayIndex ];
      var dayBoxItem = this.dayBoxItemArray[ dayIndex ];
      
      if( dayIndex < firstDayOfWeek || dayNumber > lastDayOfMonth )
      {
         // this day box is NOT in the month, 
         
         dayNumberItem.setAttribute( "value" , "" );  
         dayBoxItem.setAttribute( "empty" , "true" );  
         dayBoxItem.dayNumber = null;
      }  
      else
      {
         dayNumberItem.setAttribute( "value" , dayNumber );
         
         dayBoxItem.setAttribute( "empty" , "false" );  
         dayBoxItem.dayNumber = dayNumber;
         
         this.dayBoxItemByDateArray[ dayNumber ] = dayBoxItem; 
         ++dayNumber;  
      }
   }
  
   // if we aren't showing an event, highlite the selected date.
   if ( !ShowEvent ) 
   {
      //debug("\n\n-->>>>Im' going to be highlightin the date from refresh display!!!");
      this.hiliteSelectedDate( );
   }
   
   //always highlight today's date.
   this.hiliteTodaysDate( );
}


/** PRIVATE
*
*   Mark the selected date, also unmark the old selection if there was one
*/

MonthView.prototype.hiliteSelectedDate = function( )
{
   // Clear the old selection if there was one
   
   if( this.selectedBox )
   { 
      this.selectedBox.setAttribute( "selected" , "false" );
      this.selectedBox = null;
   }
   
   // Set the background for selection
   
   this.selectedBox = this.dayBoxItemByDateArray[ this.calendarWindow.getSelectedDate().getDate() ];
   this.selectedBox.setAttribute( "selected" , "true" );
}


/** PUBLIC
*
*  Unmark the selected date if there is one.
*/

MonthView.prototype.clearSelectedDate = function( )
{
   if ( this.selectedBox ) 
   {
      this.selectedBox.setAttribute( "selected", "false" );
      this.selectedBox = null;
   }
}

/** PRIVATE
*
*  Mark today as selected, also unmark the old today if there was one.
*/

MonthView.prototype.hiliteTodaysDate = function( )
{
   var Month = this.calendarWindow.getSelectedDate().getMonth();
   
   var Year = this.calendarWindow.getSelectedDate().getFullYear();

   // Clear the old selection if there was one
   if ( this.selectedTodayBox ) 
   {
      this.selectedTodayBox.setAttribute( "today", "false" );
      this.selectedTodayBox = null;
   }

   //highlight today.
   var Today = new Date( );
   
   if ( Year == Today.getFullYear() && Month == Today.getMonth() ) 
   {
      this.selectedTodayBox = this.dayBoxItemByDateArray[ Today.getDate() ];
      
      this.selectedTodayBox.setAttribute( "today", "true" );
   }
   else
      this.selectedTodayBox = null;
      
}


/** PUBLIC
*
*   Get the selected event and delete it.
*/

MonthView.prototype.deletedSelectedEvent = function( )
{
   if( this.selectedEventBoxes.length > 0 )
   {
      for ( i = 0; i < this.selectedEventBoxes.length; i++ ) 
      {
         var calendarEvent =  this.selectedEventBoxes[i].calendarEvent;
      
         // tell the event source to delete it, the observers will be called
         // back into to update the display
      }
      this.calendarWindow.eventSource.deleteEvent( calendarEvent );
   }
}


/** PUBLIC
*
*   This is called when we are about the make a new event
*   and we want to know what the default start date should be for the event.
*/

MonthView.prototype.getNewEventDate = function( )
{
   // use the selected year, month and day
   // and the current hours and minutes
   
   var now = new Date();
   var start = new Date( this.calendarWindow.getSelectedDate() );
   
   start.setHours( now.getHours() );
   start.setMinutes( Math.ceil( now.getMinutes() / 5 ) * 5 );
   start.setSeconds( 0 );
   
   return start;     
}


/** PUBLIC
*
*   Go to the next month.
*/

MonthView.prototype.goToNext = function()
{  
   var nextMonth = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth() + 1, 1 );
   
   this.adjustNewMonth( nextMonth );  
   
   // move to new date
   
   this.goToDay( nextMonth );
}


/** PUBLIC
*
*   Go to the previous month.
*/

MonthView.prototype.goToPrevious = function()
{
   var prevMonth = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth() - 1, 1 );
   
   this.adjustNewMonth( prevMonth );  
   
   this.goToDay( prevMonth );
   
}

/** PRIVATE
*
*   Helper function for goToNext and goToPrevious
*
*   When the user changes to a new month the new month may not have the selected day in it.
*   ( i.e. 31 was selected and the new month has only 30 days ). 
*   In that case our addition, or subtraction, in  goToNext or goToPrevious will cause the
*   date to jump a month. ( Say the date starts at May 31, we add 1 to the month, Now the
*   date would be June 31, but the Date object knows there is no June 31, so it sets itself
*   to July 1. )
*
*   In goToNext or goToPrevious we set the date to be 1, so the month will be correct. Here
*   we set the date to be the selected date, making adjustments if the selected date is not in the month.
*/

MonthView.prototype.adjustNewMonth = function( newMonth )
{
   // Don't let a date beyond the end of the month make us jump
   // too many or too few months
  
   var lastDayOfMonth = DateUtils.getLastDayOfMonth( newMonth.getFullYear(), newMonth.getMonth() );
   
   if( this.calendarWindow.selectedDate.getDate() > lastDayOfMonth )
   {
      // The selected date is NOT in the month
      // set it to the last day of the month and turn on showingLastDay, see notes in MonthView class
      
      newMonth.setDate( lastDayOfMonth )
      
      this.showingLastDay = true;
   }
   else if( this.showingLastDay )
   {
      // showingLastDay is on so select the last day of the month, see notes in MonthView class
      
      newMonth.setDate( lastDayOfMonth )
   }
   else
   {
      // date is NOT beyond the last.
      
      newMonth.setDate(  this.calendarWindow.selectedDate.getDate() )
   }
}




/** PUBLIC  -- monthview only
*
*   Called when a day box item is single clicked
*/

MonthView.prototype.clickDay = function( dayBoxItem )
{
   if( dayBoxItem.dayNumber != null )
   {
      // turn off showingLastDay - see notes in MonthView class
      
      this.showingLastDay = false;
   
      // change the selected date and redraw it
      
      this.calendarWindow.selectedDate.setDate( dayBoxItem.dayNumber );
      
      this.hiliteSelectedDate( );

      this.clearSelectedEvent( );
   }
}

MonthView.prototype.doubleClickDay = function( dayBoxItem )
{
   if ( dayBoxItem.dayNumber != null ) 
   {
      // change the selected date and redraw it

      gCalendarWindow.selectedDate.setDate( dayBoxItem.dayNumber );

      this.hiliteSelectedDate( );

      this.clearSelectedEvent( );
      
      var startDate = this.getNewEventDate();
      
      newEvent( startDate, false );

   }
}


/** PUBLIC  -- monthview only
*
*   Called when an event box item is single clicked
*/

MonthView.prototype.clickEventBox = function( eventBox, event )
{
   //deselect the selected day in the month view
   
   this.clearSelectedDate( );

   //set the selected date to the start date of this event.
   if( eventBox) 
   {
      this.calendarWindow.selectedDate.setDate( eventBox.calendarEvent.start.getDate() );
      
      // clear the old selected box
      
      this.clearSelectedEvent( );
   
      // select the event
      
      this.calendarWindow.setSelectedEvent( eventBox.calendarEvent );
      
      // mark new box as selected
      
      var ArrayOfBoxes = document.getElementsByAttribute( "name", "month-view-event-box-"+eventBox.calendarEvent.id );
   
      for ( i = 0; i < ArrayOfBoxes.length; i++ ) 
      {
         ArrayOfBoxes[i].setAttribute( "selected", "true" );
         this.selectedEventBoxes[ this.selectedEventBoxes.length ] = ArrayOfBoxes[i];
      }
   
      // Do not let the click go through, suppress default selection
      
      if ( event ) 
      {
         event.stopPropagation();
      }
      
      //select the event in the unifinder
   
      selectEventInUnifinder( eventBox.calendarEvent );
   }
   

}

MonthView.prototype.selectEvent = function( calendarEvent )
{
   //clear the selected event
   gCalendarWindow.clearSelectedEvent( );
   
   gCalendarWindow.setSelectedEvent( calendarEvent );
   
   var EventBoxes = document.getElementsByAttribute( "name", "week-view-event-box-"+calendarEvent.id );
   
   for ( i = 0; i < EventBoxes.length; i++ ) 
   {
      EventBoxes[i].setAttribute( "selected", "true" );
      
      this.selectedEventBoxes[ this.selectedEventBoxes.length ] = EventBoxes[i];
   }
   

   selectEventInUnifinder( calendarEvent );
}

MonthView.prototype.clearSelectedEvent = function ( )
{
   Event = gCalendarWindow.getSelectedEvent();
   
   //if ( Event && document.getElementById( "month-view-event-box-"+Event.id ) )
   if ( Event && document.getElementsByAttribute( "name", "month-view-event-box-"+Event.id ).length > 0 )
   {
      ArrayOfElements = document.getElementsByAttribute( "id", "month-view-event-box-"+Event.id );
      for ( i = 0; i < ArrayOfElements.length; i++ ) 
      {
         ArrayOfElements[i].setAttribute( "selected", false );
      }
   }
   
   this.selectedEventBoxes = Array();

   //clear the selection in the unifinder
   deselectEventInUnifinder( );

}


MonthView.prototype.getVisibleEvent = function( calendarEvent )
{
   eventBox = document.getElementById( "month-view-event-box-"+calendarEvent.id );
   if ( eventBox ) 
   {
      return eventBox;
   }
   else
      return null;

}
