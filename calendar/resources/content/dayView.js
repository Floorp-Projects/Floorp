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
*   DayView Class  subclass of CalendarView
*
*  Calendar day view class
*
* PROPERTIES
*     dayEvents     - Event text is displayed in the hour items of the view. The items that currently
*                     have an event are stored here. The hour items have a calendarEvent property 
*                     added so we know which event is displayed for the day.
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
   
   this.selectedEventBoxes = new Array();
   
   // set up dayEvents array
   this.dayEvents = new Array(); 

}


/** PUBLIC
*
*   Redraw the events for the current month
* 
*/

DayView.prototype.refreshEvents = function( )
{
   this.kungFooDeathGripOnEventBoxes = new Array();

   // remove old event boxes
   
   var eventBoxList = document.getElementsByAttribute( "eventbox", "dayview" );

   for( var eventBoxIndex = 0;  eventBoxIndex < eventBoxList.length; ++eventBoxIndex )
   {
      var eventBox = eventBoxList[ eventBoxIndex ];
      
      eventBox.parentNode.removeChild( eventBox );
   }
   
   //get the all day box.
   
   var AllDayBox = document.getElementById( "all-day-content-box" );
   AllDayBox.setAttribute( "collapsed", "true" );      
   
   //remove all the text from the all day content box.
   
   while( AllDayBox.hasChildNodes() )
   {
      AllDayBox.removeChild( AllDayBox.firstChild );
   }

   //shrink the day's content box.
   
   document.getElementById( "day-view-content-box" ).setAttribute( "allday", "false" );

   //make the text node that will contain the text for the all day box.
   
   //var TextNode = document.getElementById( "all-day-content-box-text" );
   //if ( TextNode == null ) 
   //{
      HtmlNode = document.createElement( "description" );
      HtmlNode.setAttribute( "class", "all-day-content-box-text-title" );
      TextNode = document.createTextNode( "All-Day Events" );
      HtmlNode.appendChild( TextNode );
      document.getElementById( "all-day-content-box" ).appendChild( HtmlNode );
      
   //}
   //TextNode.setAttribute( "value", "All-Day Events: " );
   
   //set the seperator to nothing. After the first one, we'll change it to ", " so that we add commas between text.
   
   var Seperator = " ";

   // get the events for the day and loop through them
   
   var dayEventList = this.calendarWindow.eventSource.getEventsForDay( this.calendarWindow.getSelectedDate() );
  
   //refresh the array and the current spot.
   
   for ( i = 0; i < dayEventList.length; i++ ) 
   {
      dayEventList[i].OtherSpotArray = new Array('0');
      dayEventList[i].CurrentSpot = 0;
      dayEventList[i].NumberOfSameTimeEvents = 0;
   }

   for ( i = 0; i < dayEventList.length; i++ ) 
   {
      var calendarEventDisplay = dayEventList[i];
            
      //check to make sure that the event is not an all day event...
      if ( calendarEventDisplay.event.allDay != true ) 
      {
         //see if there's another event at the same start time.
         
         for ( j = 0; j < dayEventList.length; j++ ) 
         {
            thisCalendarEventDisplay = dayEventList[j];
   
            //if this event overlaps with another event...
            if ( ( ( thisCalendarEventDisplay.displayDate >= calendarEventDisplay.displayDate &&
                 thisCalendarEventDisplay.displayDate.getTime() < calendarEventDisplay.event.end.getTime() ) ||
                  ( calendarEventDisplay.displayDate >= thisCalendarEventDisplay.displayDate &&
                 calendarEventDisplay.displayDate.getTime() < thisCalendarEventDisplay.event.end.getTime() ) ) &&
                 calendarEventDisplay.event.id != thisCalendarEventDisplay.event.id &&
                 thisCalendarEventDisplay.event.allDay != true )
            {
                  //get the spot that this event will go in.
               var ThisSpot = thisCalendarEventDisplay.CurrentSpot;
               
               calendarEventDisplay.OtherSpotArray.push( ThisSpot );
               ThisSpot++;
               
               if ( ThisSpot > calendarEventDisplay.CurrentSpot ) 
               {
                  calendarEventDisplay.CurrentSpot = ThisSpot;
               } 
            }
         }
         SortedOtherSpotArray = new Array();
         SortedOtherSpotArray = calendarEventDisplay.OtherSpotArray.sort( gCalendarWindow.compareNumbers );
         LowestNumber = this.calendarWindow.getLowestElementNotInArray( SortedOtherSpotArray );
         
         //this is the actual spot (0 -> n) that the event will go in on the day view.
         calendarEventDisplay.CurrentSpot = LowestNumber;
         calendarEventDisplay.NumberOfSameTimeEvents = SortedOtherSpotArray.length;
  
      }

   }
   
   for ( var eventIndex = 0; eventIndex < dayEventList.length; ++eventIndex )
   {
      calendarEventDisplay = dayEventList[ eventIndex ];

      //if its an all day event, don't show it in the hours stack.
      if ( calendarEventDisplay.event.allDay == true ) 
      {
         // build up the text to show for this event
         
         var eventText = calendarEventDisplay.event.title;
         
         if( calendarEventDisplay.event.location )
         {
            eventText += " " + calendarEventDisplay.event.location;
         }
            
         if( calendarEventDisplay.event.description )
         {
            eventText += " " + calendarEventDisplay.event.description;
         }
         
         //show the all day box 
         AllDayBox.setAttribute( "collapsed", "false" );
         
         //shrink the day's content box.
         document.getElementById( "day-view-content-box" ).setAttribute( "allday", "true" );
         
         //note the use of the AllDayText Attribute.  
         //This is used to remove the text when the day is changed.
         
         SeperatorNode = document.createElement( "label" );
         SeperatorNode.setAttribute( "value", Seperator );
         //SeperatorNode.setAttribute( "AllDayText", "true" );

         newTextNode = document.createElement( "label" );
         newTextNode.setAttribute( "value", eventText );
         newTextNode.calendarEventDisplay = calendarEventDisplay;
         newTextNode.setAttribute( "onmouseover", "gCalendarWindow.mouseOverInfo( calendarEventDisplay, event )" );
         newTextNode.setAttribute( "onclick", "dayEventItemClick( this, event )" );
         newTextNode.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
         newTextNode.setAttribute( "tooltip", "savetip" );
         
         //newTextNode.setAttribute( "AllDayText", "true" );
         
         newImage = document.createElement("image");
         newImage.setAttribute( "src", "chrome://calendar/skin/all_day_event.png" );
         newImage.calendarEventDisplay = calendarEventDisplay;
         newImage.setAttribute( "onmouseover", "gCalendarWindow.mouseOverInfo( calendarEventDisplay, event )" );
         newImage.setAttribute( "onclick", "dayEventItemClick( this, event )" );
         newImage.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
         newImage.setAttribute( "tooltip", "savetip" );
         
         //newImage.setAttribute( "AllDayText", "true" );
         
         AllDayBox.appendChild( SeperatorNode );
         AllDayBox.appendChild( newImage );
         AllDayBox.appendChild( newTextNode );
         
         //change the seperator to add commas after the text.
         Seperator = ", ";
      }
      else
      {
         eventBox = this.createEventBox( calendarEventDisplay );    
         
         //add the box to the stack.
         document.getElementById( "day-view-content-board" ).appendChild( eventBox );
      }

   }
      
   // select the hour of the selected item, if there is an item whose date matches
   
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
DayView.prototype.createEventBox = function ( calendarEventDisplay )
{
   
   // build up the text to show for this event

   var eventText = calendarEventDisplay.event.title;
      
   var eventStartDate = calendarEventDisplay.displayDate;
   var startHour = eventStartDate.getHours();
   var startMinutes = eventStartDate.getMinutes();

   var eventEndDateTime = new Date( 2000, 1, 1, calendarEventDisplay.event.end.hour, calendarEventDisplay.event.end.minute, 0 );
   var eventStartDateTime = new Date( 2000, 1, 1, eventStartDate.getHours(), eventStartDate.getMinutes(), 0 );

   var eventDuration = new Date( eventEndDateTime - eventStartDateTime );
   
   var hourDuration = eventDuration / (3600000);
   
   /*if( calendarEventDisplay.event.location )
   {
      eventText += "\n" + calendarEventDisplay.event.location;
   }
      
   if(  calendarEventDisplay.event.description )
   {
      eventText += "\n" + calendarEventDisplay.event.description;
   }
   */
      
   var eventBox = document.createElement( "hbox" );
   
   var topHeight = eval( ( startHour*kDayViewHourHeight ) + ( ( startMinutes/60 ) * kDayViewHourHeight ) );
   topHeight = Math.round( topHeight ) - 1;
   eventBox.setAttribute( "top", topHeight );
   eventBox.setAttribute( "height", Math.round( ( hourDuration*kDayViewHourHeight ) + 1 ) );
   var width = Math.round( 500 / calendarEventDisplay.NumberOfSameTimeEvents );
   eventBox.setAttribute( "width", width );
   eventBox.setAttribute( "style", "max-width: "+width+"px;" );
   var left = eval( ( ( calendarEventDisplay.CurrentSpot - 1 ) * width )  + kDayViewHourLeftStart );
   left = left - ( 1 * ( calendarEventDisplay.CurrentSpot - 1 ));
   eventBox.setAttribute( "left", Math.round( left ) );
   eventBox.setAttribute( "style", "max-width: "+width+";max-height: "+eventBox.getAttribute( "height" )+";overflow: never;" );

   eventBox.setAttribute( "class", "day-view-event-class" );
   eventBox.setAttribute( "flex", "1" );
   eventBox.setAttribute( "eventbox", "dayview" );
   eventBox.setAttribute( "onclick", "dayEventItemClick( this, event )" );
   eventBox.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
   eventBox.setAttribute( "onmouseover", "gCalendarWindow.mouseOverInfo( calendarEventDisplay, event )" );
   eventBox.setAttribute( "tooltip", "savetip" );
   eventBox.setAttribute( "id", "day-view-event-box-"+calendarEventDisplay.event.id );
   eventBox.setAttribute( "name", "day-view-event-box-"+calendarEventDisplay.event.id );

   var eventHTMLElement = document.createElement( "description" );
   eventHTMLElement.setAttribute( "id", "day-view-event-html"+calendarEventDisplay.event.id );
   var eventTextElement = document.createTextNode( eventText );
   eventHTMLElement.setAttribute( "class", "day-view-event-text-class" );
   
   eventHTMLElement.appendChild( eventTextElement );
   
   eventBox.appendChild( eventHTMLElement );

   // add a property to the event box that holds the calendarEvent that the
   // box represents
   
   eventBox.calendarEventDisplay = calendarEventDisplay;
   
   this.kungFooDeathGripOnEventBoxes.push( eventBox );
         
   return( eventBox );

}


/** PUBLIC
*
*   Called when the user switches from a different view
*/

DayView.prototype.switchFrom = function( )
{
}


/** PUBLIC
*
*   Called when the user switches to the day view
*/

DayView.prototype.switchTo = function( )
{
   // disable/enable view switching buttons   

   var weekViewButton = document.getElementById( "calendar-week-view-button" );
   var monthViewButton = document.getElementById( "calendar-month-view-button" );
   var dayViewButton = document.getElementById( "calendar-day-view-button" );
   
   monthViewButton.setAttribute( "disabled", "false" );
   weekViewButton.setAttribute( "disabled", "false" );
   dayViewButton.setAttribute( "disabled", "true" );

   // switch views in the deck
   
   var calendarDeckItem = document.getElementById( "calendar-deck" );
   

   calendarDeckItem.setAttribute( "selectedIndex", 2 );
}


/** PUBLIC
*
*   Redraw the display, but not the events
*/

DayView.prototype.refreshDisplay = function( )
{
   // update the title
   
   var dayName = this.calendarWindow.dateFormater.getDayName( this.calendarWindow.getSelectedDate().getDay() );
   var monthName = this.calendarWindow.dateFormater.getMonthName( this.calendarWindow.getSelectedDate().getMonth() );
   
   var dateString = dayName + ", " + monthName + " " + this.calendarWindow.getSelectedDate().getDate() + " " + this.calendarWindow.getSelectedDate().getFullYear();
   
   var dayTextItem = document.getElementById( "day-title-text" );
   dayTextItem.setAttribute( "value" , dateString );  
}



/** PUBLIC
*
*   Get the selected event and delete it.
*/

DayView.prototype.deletedSelectedEvent = function( )
{
   // :TODO: 
}
 


/** PUBLIC  -- monthview only
*
*   Called when an event box item is single clicked
*/

DayView.prototype.clickEventBox = function( eventBox, event )
{
   // clear the old selected box
   
   for ( i = 0; i < this.selectedEventBoxes.length; i++ ) 
   {
      this.selectedEventBoxes[i].setAttribute( "selected", false );
   }
   
   this.selectedEventBoxes = Array();

   if( eventBox )
   {
      // select the event
      this.selectEvent( eventBox.calendarEventDisplay.event );
         
      selectEventInUnifinder( eventBox.calendarEventDisplay.event );
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

DayView.prototype.getNewEventDate = function( )
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

DayView.prototype.goToNext = function()
{
   var nextDay = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(),  this.calendarWindow.selectedDate.getDate() + 1 );
   
   this.goToDay( nextDay );
}


/** PUBLIC
*
*   Go to the previous day.
*/

DayView.prototype.goToPrevious = function()
{
   var prevDay = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth(),  this.calendarWindow.selectedDate.getDate() - 1 );
   
   this.goToDay( prevDay );
}


/** PUBLIC
*
*   select an event.
*/
DayView.prototype.selectEvent = function( calendarEvent )
{
   //clear the selected event
   gCalendarWindow.clearSelectedEvent( );
   
   gCalendarWindow.setSelectedEvent( calendarEvent );
   
   EventBoxes = document.getElementsByAttribute( "name", "day-view-event-box-"+calendarEvent.id );
   
   for ( i = 0; i < EventBoxes.length; i++ ) 
   {
      EventBoxes[i].setAttribute( "selected", "true" );

      this.selectedEventBoxes[ this.selectedEventBoxes.length ] = EventBoxes[i];
   }
      
   selectEventInUnifinder( calendarEvent );
}


/** PUBLIC
*
*   clear the selected event by taking off the selected attribute.
*/
DayView.prototype.clearSelectedEvent = function( )
{
   //Event = gCalendarWindow.getSelectedEvent();
   
   //if there is an event, and if the event is in the current view.
   //var SelectedBoxes = document.getElementsByAttribute( "name", "day-view-event-box-"+Event.id );

   for ( i = 0; i < this.selectedEventBoxes.length; i++ ) 
   {
      this.selectedEventBoxes[i].setAttribute( "selected", false );
   }
   
   this.selectedEventBoxes = Array();

   //clear the selection in the unifinder
   deselectEventInUnifinder( );
}


DayView.prototype.clearSelectedDate = function( )
{
   return;
}


DayView.prototype.getVisibleEvent = function( calendarEvent )
{
   eventBox = document.getElementById( "day-view-event-box-"+calendarEvent.id );
   if ( eventBox ) 
   {
      return eventBox;
   }
   else
      return null;

}

/*
** This function is needed because it may be called after the end of each day.
*/

DayView.prototype.hiliteTodaysDate = function( )
{
   return;
}
