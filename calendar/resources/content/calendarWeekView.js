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
*   WeekView Class  subclass of CalendarView
*
*  Calendar week view class    :TODO: display of events has not been started
*
* PROPERTIES
*     gHeaderDateItemArray   - Array of text boxes used to display the dates of the currently displayed
*                             week.
*   
* 
*/



// Make WeekView inherit from CalendarView

WeekView.prototype = new CalendarView();
WeekView.prototype.constructor = WeekView;

/**
*   WeekView Constructor.
* 
* PARAMETERS
*      calendarWindow     - the owning instance of CalendarWindow.
*
*/


function WeekView( calendarWindow )
{
   // call super 
   
   this.superConstructor( calendarWindow );
   
   // get week view header items
   
   gHeaderDateItemArray = new Array();
   
   this.selectedEventBoxes = new Array();
   
   for( var dayIndex = 1; dayIndex <= 7; ++dayIndex )
   {
      var headerDateItem = document.getElementById( "week-header-date-" + dayIndex );
      
      gHeaderDateItemArray[ dayIndex ] = headerDateItem;
   }
}


/** PUBLIC
*
*   Redraw the events for the current week
*/

WeekView.prototype.refreshEvents = function( )
{
   this.kungFooDeathGripOnEventBoxes = new Array();
   
   var eventBoxList = document.getElementsByAttribute( "eventbox", "weekview" );

   for( var eventBoxIndex = 0;  eventBoxIndex < eventBoxList.length; ++eventBoxIndex )
   {
      var eventBox = eventBoxList[ eventBoxIndex ];
      
      eventBox.parentNode.removeChild( eventBox );
   }

   for( var dayIndex = 1; dayIndex <= 7; ++dayIndex )
   {
      var headerDateItem = document.getElementById( "week-header-date-" + dayIndex );
      
      headerDateItem.numEvents = 0;
   }
   
   //get the all day box.
   
   var AllDayBox = document.getElementById( "week-view-all-day-content-box" );
   AllDayBox.setAttribute( "collapsed", "true" );      
   
   //expand the day's content box by setting allday to false..
   
   document.getElementById( "week-view-content-box" ).setAttribute( "allday", "false" );

   //START FOR LOOP FOR DAYS---> 
   for ( dayIndex = 1; dayIndex <= 7; ++dayIndex )
   {

      //make the text node that will contain the text for the all day box.
      
      var TextNode = document.getElementById( "all-day-content-box-text-week-"+dayIndex );
      
      //set the seperator to nothing. After the first one, we'll change it to ", " so that we add commas between text.
      
      var Seperator = "";
   
      // get the events for the day and loop through them
      dayToGet = new Date( gHeaderDateItemArray[dayIndex].getAttribute( "date" ) );
      
      var dayEventList = new Array();
      
      dayEventList = this.calendarWindow.eventSource.getEventsForDay( dayToGet );
     
      //refresh the array and the current spot.
      
      for ( i = 0; i < dayEventList.length; i++ ) 
      {
         dayEventList[i].OtherSpotArray = new Array('0');
         dayEventList[i].CurrentSpot = 0;
         dayEventList[i].NumberOfSameTimeEvents = 0;
      }
   
      for ( i = 0; i < dayEventList.length; i++ ) 
      {
         var calendarEvent = dayEventList[i];
         
         //check to make sure that the event is not an all day event...
         
         if ( calendarEvent.allDay != true ) 
         {
            //see if there's another event at the same start time.
            
            for ( j = 0; j < dayEventList.length; j++ ) 
            {
               thisCalendarEvent = dayEventList[j];
   
               //if this event overlaps with another event...
               if ( ( ( thisCalendarEvent.displayDate >= calendarEvent.displayDate &&
                    thisCalendarEvent.displayDate < calendarEvent.end ) ||
                     ( calendarEvent.displayDate >= thisCalendarEvent.displayDate &&
                    calendarEvent.displayDate < thisCalendarEvent.end ) ) &&
                    calendarEvent.id != thisCalendarEvent.id )
               {
                  //get the spot that this event will go in.
                  var ThisSpot = thisCalendarEvent.CurrentSpot;
                  
                  calendarEvent.OtherSpotArray.push( ThisSpot );
                  ThisSpot++;
                  
                  if ( ThisSpot > calendarEvent.CurrentSpot ) 
                  {
                     calendarEvent.CurrentSpot = ThisSpot;
                  } 
               }
            }
            SortedOtherSpotArray = new Array();
            SortedOtherSpotArray = calendarEvent.OtherSpotArray.sort( gCalendarWindow.compareNumbers);
            LowestNumber = this.calendarWindow.getLowestElementNotInArray( SortedOtherSpotArray );
            
            //this is the actual spot (0 -> n) that the event will go in on the day view.
            calendarEvent.CurrentSpot = LowestNumber;
            if ( SortedOtherSpotArray.length > 4 ) 
            {
               calendarEvent.NumberOfSameTimeEvents = 4;
            }
            else
            {
               calendarEvent.NumberOfSameTimeEvents = SortedOtherSpotArray.length;
            }
            
         dayEventList[i] = calendarEvent;
         }
   
      }
      var ThisDayAllDayBox = document.getElementById( "all-day-content-box-week-"+dayIndex );
      
      while ( ThisDayAllDayBox.hasChildNodes() ) 
      {
         ThisDayAllDayBox.removeChild( ThisDayAllDayBox.firstChild );
      }
      
      for ( var eventIndex = 0; eventIndex < dayEventList.length; ++eventIndex )
      {
         var calendarEvent = dayEventList[ eventIndex ];
   
         var eventDate = calendarEvent.start;
      
         // get the day box for the calendarEvent's day
         
         var eventDayInMonth = eventDate.getDate();
         
         var weekBoxItem = gHeaderDateItemArray[ dayIndex ];
               
         // Display no more than three, show dots for the events > 3
         
         if ( calendarEvent.allDay != true ) 
         {
            weekBoxItem.numEvents +=  1;
         }
         
         //if its an all day event, don't show it in the hours stack.
         
         if ( calendarEvent.allDay == true ) 
         {
            // build up the text to show for this event
            
            var eventText = calendarEvent.title;
            
            if( calendarEvent.location )
            {
               eventText += " " + calendarEvent.location;
            }
               
            if( calendarEvent.description )
            {
               eventText += " " + calendarEvent.description;
            }
            
            //show the all day box 
            AllDayBox.setAttribute( "collapsed", "false" );
            
            //shrink the day's content box.
            document.getElementById( "week-view-content-box" ).setAttribute( "allday", "true" );
            
            //note the use of the WeekViewAllDayText Attribute.  
            //This is used to remove the text when the day is changed.
            
            SeperatorNode = document.createElement( "label" );
            SeperatorNode.setAttribute( "value", Seperator );
            SeperatorNode.setAttribute( "WeekViewAllDayText", "true" );
   
            newHTMLNode = document.createElement( "description" );
                        
            newTextNode = document.createTextNode( eventText );
            //newTextNode.setAttribute( "value", eventText );
            newHTMLNode.appendChild( newTextNode );
            newHTMLNode.setAttribute( "WeekViewAllDayText", "true" );
            
            newImage = document.createElement("image");
            newImage.setAttribute( "src", "chrome://calendar/skin/all_day_event.png" );
            newImage.setAttribute( "WeekViewAllDayText", "true" );
            newImage.calendarEvent = calendarEvent;
            newImage.setAttribute( "onmouseover", "gCalendarWindow.mouseOverInfo( calendarEvent, event )" );
            newImage.setAttribute( "tooltip", "savetip" );
         

            //ThisDayAllDayBox.appendChild( SeperatorNode );
            ThisDayAllDayBox.appendChild( newImage );
            //ThisDayAllDayBox.appendChild( newHTMLNode );
            
            //change the seperator to add commas after the text.
            Seperator = ", ";
         }
         else if ( calendarEvent.CurrentSpot <= 4 ) 
         {
            eventBox = this.createEventBox( calendarEvent, dayIndex );    
            
            //add the box to the stack.
            document.getElementById( "week-view-content-board" ).appendChild( eventBox );
         }
      }
         
   }
   //--> END THE FOR LOOP FOR THE WEEK VIEW

   var selectedEvent = this.calendarWindow.getSelectedEvent();

   //if the selected event is today, highlight it.  Otherwise, don't highlight anything.
   if ( selectedEvent ) 
   {
      this.selectEvent( selectedEvent );
   }
}


/** PRIVATE
*
*   This creates an event box for the day view
*/
WeekView.prototype.createEventBox = function ( calendarEvent, dayIndex )
{
   
   // build up the text to show for this event

   var eventText = calendarEvent.title;
      
   var eventStartDate = calendarEvent.displayDate;
   var startHour = eventStartDate.getHours();
   var startMinutes = eventStartDate.getMinutes();

   var eventEndDate = calendarEvent.end;
   var endHour = eventEndDate.getHours();

   var eventEndDateTime = new Date( 2000, 1, 1, eventEndDate.getHours(), eventEndDate.getMinutes(), 0 );
   var eventStartDateTime = new Date( 2000, 1, 1, eventStartDate.getHours(), eventStartDate.getMinutes(), 0 );

   var eventDuration = new Date( eventEndDateTime - eventStartDateTime );
   
   var hourDuration = eventDuration / (3600000);
   
   var eventBox = document.createElement( "vbox" );
   
   eventBox.calendarEvent = calendarEvent;
   
   var Height = ( hourDuration * kWeekViewHourHeight ) + 1;
   var Width = ( 80 / calendarEvent.NumberOfSameTimeEvents ) - 1;
   eventBox.setAttribute( "height", Height );
   eventBox.setAttribute( "width", Width );
      
   top = eval( ( startHour*kWeekViewHourHeight ) + ( ( startMinutes/60 ) * kWeekViewHourHeight ) - kWeekViewHourHeightDifference );
   eventBox.setAttribute( "top", top );
   
   left = eval( kWeekViewHourLeftStart + ( kWeekViewHourWidth * ( dayIndex - 1 ) ) + ( ( calendarEvent.CurrentSpot - 1 ) * eventBox.getAttribute( "width" ) ) + 1 );
   eventBox.setAttribute( "left", left );
   
   eventBox.setAttribute( "class", "week-view-event-class" );
   eventBox.setAttribute( "style", "max-width: "+Width+"; max-height: "+Height+";" );
   eventBox.setAttribute( "flex", "0" );
   eventBox.setAttribute( "eventbox", "weekview" );
   eventBox.setAttribute( "onclick", "weekEventItemClick( this, event )" );
   eventBox.setAttribute( "ondblclick", "weekEventItemDoubleClick( this, event )" );
   eventBox.setAttribute( "id", "week-view-event-box-"+calendarEvent.id );
   eventBox.setAttribute( "name", "week-view-event-box-"+calendarEvent.id );
   eventBox.setAttribute( "onmouseover", "gCalendarWindow.mouseOverInfo( calendarEvent, event )" );
   eventBox.setAttribute( "tooltip", "savetip" );
         
   /*
   ** REMOVE THE EVENT TEXT FOR NOW 
   ** 
   ** THIS REQUIRES THE DESCRIPTION XUL ELEMENT, WHICH WE MISSED BY 9 DAYS FOR BETA
   */
   /*
   var eventDescriptionElement = document.createElement( "description" );
   var DescriptionText = document.createTextNode( "This is my new text" );
   eventDescriptionElement.appendChild( DescriptionText );
   eventDescriptionElement.setAttribute( "style", "border: 1px solid red; height: "+Height+";" );
   eventDescriptionElement.crop = "end";
   eventBox.appendChild( eventDescriptionElement );
   */
   
   // add a property to the event box that holds the calendarEvent that the
   // box represents
   
   this.kungFooDeathGripOnEventBoxes.push( eventBox );
         
   return( eventBox );

}


/** PUBLIC
*
*   Called when the user switches to a different view
*/

WeekView.prototype.switchFrom = function( )
{
}


/** PUBLIC
*
*   Called when the user switches to the week view
*/

WeekView.prototype.switchTo = function( )
{
   // disable/enable view switching buttons   

   var weekViewButton = document.getElementById( "calendar-week-view-button" );
   var monthViewButton = document.getElementById( "calendar-month-view-button" );
   var dayViewButton = document.getElementById( "calendar-day-view-button" );
   
   monthViewButton.setAttribute( "disabled", "false" );
   weekViewButton.setAttribute( "disabled", "true" );
   dayViewButton.setAttribute( "disabled", "false" );

   // switch views in the deck

   var calendarDeckItem = document.getElementById( "calendar-deck" );
   calendarDeckItem.setAttribute( "selectedIndex", 1 );
}


/** PUBLIC
*
*   Redraw the display, but not the events
*/

WeekView.prototype.refreshDisplay = function( )
{
   
   // Set the from-to title string, based on the selected date
   
   var viewDay = this.calendarWindow.getSelectedDate().getDay();
   var viewDayOfMonth = this.calendarWindow.getSelectedDate().getDate();
   var viewMonth = this.calendarWindow.getSelectedDate().getMonth();
   var viewYear = this.calendarWindow.getSelectedDate().getFullYear();
   
   var daysLeftInWeek = 6 - viewDay;
   var dateOfLastDayInWeek = viewDayOfMonth + daysLeftInWeek;
   
   var dateOfFirstDayInWeek = viewDayOfMonth - viewDay;
   
   var firstDayOfWeek = new Date( viewYear, viewMonth, dateOfFirstDayInWeek );
   var lastDayOfWeek = new Date( viewYear, viewMonth, dateOfLastDayInWeek );
    
   var firstDayMonthName = this.calendarWindow.dateFormater.getMonthName( firstDayOfWeek.getMonth() );
   var lastDayMonthName =  this.calendarWindow.dateFormater.getMonthName( lastDayOfWeek.getMonth() );
   
   var dateString = firstDayMonthName + " " + firstDayOfWeek.getDate() + " - " +
                    lastDayMonthName  + " " + lastDayOfWeek.getDate();
   
   var weekTextItem = document.getElementById( "week-title-text" );
   weekTextItem.setAttribute( "value" , dateString ); 
   
   // Set the dates in the header
   
   var weekDate = new Date( viewYear, viewMonth, dateOfFirstDayInWeek );
   
   for( var dayIndex = 1; dayIndex < gHeaderDateItemArray.length; ++dayIndex )
   {
      var dateOfDay = weekDate.getDate();
      
      var headerDateItem = gHeaderDateItemArray[ dayIndex ];
      headerDateItem.setAttribute( "value" , dateOfDay );
      headerDateItem.setAttribute( "date", weekDate );
      headerDateItem.numEvents = 0;
      // advance to next day 
      
      weekDate.setDate( dateOfDay + 1 );
   }

   this.hiliteTodaysDate( );
}


/** PUBLIC
*
*   Get the selected event and delete it.
*/

WeekView.prototype.deletedSelectedEvent = function( )
{
   // :TODO:
}
 

/** PUBLIC
*
*   This is called when we are about the make a new event
*   and we want to know what the default start date should be for the event.
*/

WeekView.prototype.getNewEventDate = function( )
{
   return this.calendarWindow.getSelectedDate();
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


WeekView.prototype.getVisibleEvent = function( calendarEvent )
{
   eventBox = document.getElementById( "week-view-event-box-"+calendarEvent.id );
   if ( eventBox ) 
   {
      return eventBox;
   }
   else
      return null;

}


/** PRIVATE
*
*  Mark today as selected, also unmark the old today if there was one.
*/

WeekView.prototype.hiliteTodaysDate = function( )
{
   //clear out the old today boxes.
   var OldTodayArray = document.getElementsByAttribute( "today", "true" );
   for ( i = 0; i < OldTodayArray.length; i++ ) 
   {
      OldTodayArray[i].setAttribute( "today", "false" );
   }
                                                      

   // get the events for the day and loop through them
   var FirstDay = new Date( gHeaderDateItemArray[1].getAttribute( "date" ) );
   var LastDay  = new Date( gHeaderDateItemArray[7].getAttribute( "date" ) );
   LastDay.setHours( 23 );
   LastDay.setMinutes( 59 );
   LastDay.setSeconds( 59 );
   var Today = new Date();

   if ( Today.getTime() > FirstDay.getTime() && Today.getTime() < LastDay.getTime() ) 
   {
      //today is visible, get the day index for today
      for ( dayIndex = 1; dayIndex <= 7; ++dayIndex )
      {
         ThisDate = new Date( gHeaderDateItemArray[dayIndex].getAttribute( "date" ) );
         if ( ThisDate.getFullYear() == Today.getFullYear()
                && ThisDate.getMonth() == Today.getMonth()
                && ThisDate.getDay()   == Today.getDay() ) 
         {
            break;
         }
      }
      //dayIndex now contains the box numbers that we need.
      for ( i = 0; i < 24; i++ ) 
      {
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
   Event = gCalendarWindow.getSelectedEvent();
   
   //if there is an event, and if the event is in the current view.
   if ( Event && document.getElementsByAttribute( "name", "week-view-event-box-"+Event.id ).length > 0 )
   {
      for ( i = 0; i < this.selectedEventBoxes.length; i++ ) 
      {
         this.selectedEventBoxes[i].setAttribute( "selected", "false" );
      }
      //document.getElementById( "week-view-event-box-"+Event.id ).setAttribute( "selected", false );
   }
   
   this.selectedEventBoxes = Array();

   //clear the selection in the unifinder
   deselectEventInUnifinder( );
}


/** PUBLIC  -- monthview only
*
*   Called when an event box item is single clicked
*/

WeekView.prototype.clickEventBox = function( eventBox, event )
{
   // clear the old selected box
   
   if( this.selectedEventBoxes.length > 0 )
   {
      this.clearSelectedEvent();
   }
   
   if( eventBox )
   {
      // select the event
      this.selectEvent( eventBox.calendarEvent );
      
      //set the selected date to the start date of this event.
   
      this.calendarWindow.selectedDate.setDate( eventBox.calendarEvent.start.getDate() );
         
      // mark new box as selected
      var ArrayOfBoxes = document.getElementsByAttribute( "name", "week-view-event-box-"+eventBox.calendarEvent.id );
      for ( i = 0; i < ArrayOfBoxes.length; i++ ) 
      {
         ArrayOfBoxes[i].setAttribute( "selected", "true" );
   
         this.selectedEventBoxes[ this.selectedEventBoxes.length ] = ArrayOfBoxes[i];
      }
      
      //select the event in the unifinder
   
      selectEventInUnifinder( eventBox.calendarEvent );


   }
      // Do not let the click go through, suppress default selection
   
   if ( event )
   {
      event.stopPropagation();
   }

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
*   select an event.
*/
WeekView.prototype.selectEvent = function( calendarEvent )
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

/** PUBLIC
*
*  Unmark the selected date if there is one.
*/

WeekView.prototype.clearSelectedDate = function( )
{
   if ( this.selectedBox ) 
   {
      this.selectedBox.setAttribute( "selected", "false" );
      this.selectedBox = null;
   }
}
