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
   
   //set the time on the left hand side labels
   //need to do this in JavaScript to preserve formatting
   for( var i = 0; i < 24; i++ )
   {
      /*
      ** FOR SOME REASON, THIS IS NOT WORKING FOR MIDNIGHT
      */ 
      var TimeToFormat = new Date();

      TimeToFormat.setHours( i );

      TimeToFormat.setMinutes( "0" );

      var FormattedTime = calendarWindow.dateFormater.getFormatedTime( TimeToFormat );

      var Label = document.getElementById( "week-view-hour-"+i );
      
      Label.setAttribute( "value", FormattedTime );
   }
   
   // get week view header items
   
   gHeaderDateItemArray = new Array();
   
   for( var dayIndex = 1; dayIndex <= 7; ++dayIndex )
   {
      var headerDateItem = document.getElementById( "week-header-date-" + dayIndex );
      
      gHeaderDateItemArray[ dayIndex ] = headerDateItem;
   }

   var weekViewEventSelectionObserver = 
   {
      onSelectionChanged : function( EventSelectionArray )
      {
         for( i = 0; i < EventSelectionArray.length; i++ )
         {
            gCalendarWindow.weekView.selectBoxForEvent( EventSelectionArray[i] );
         }
      }
   }
      
   calendarWindow.EventSelection.addObserver( weekViewEventSelectionObserver );

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
   
   document.getElementById( "week-view-content-box" ).removeAttribute( "allday" );

   //START FOR LOOP FOR DAYS---> 
   for ( dayIndex = 1; dayIndex <= 7; ++dayIndex )
   {

      //make the text node that will contain the text for the all day box.
      
      var TextNode = document.getElementById( "all-day-content-box-text-week-"+dayIndex );
      
      // get the events for the day and loop through them
      var dayToGet = new Date( gHeaderDateItemArray[dayIndex].getAttribute( "date" ) );
      
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
         var ThisSpot = 0;

         var calendarEventDisplay = dayEventList[i];
         
         //check to make sure that the event is not an all day event...
         
         if ( calendarEventDisplay.event.allDay != true ) 
         {
            //see if there's another event at the same start time.
            
            for ( var j = 0; j < dayEventList.length; j++ ) 
            {
               var thisCalendarEventDisplay = dayEventList[j];
   
               //if this event overlaps with another event...
               if ( ( ( thisCalendarEventDisplay.displayDate >= calendarEventDisplay.displayDate &&
                    thisCalendarEventDisplay.displayDate.getTime() < calendarEventDisplay.event.end.getTime() ) ||
                     ( calendarEventDisplay.displayDate >= thisCalendarEventDisplay.displayDate &&
                    calendarEventDisplay.displayDate.getTime() < thisCalendarEventDisplay.event.end.getTime() ) ) &&
                    calendarEventDisplay.event.id != thisCalendarEventDisplay.event.id &&
                    thisCalendarEventDisplay.event.allDay != true )
               {
                  //get the spot that this event will go in.
                  ThisSpot = thisCalendarEventDisplay.CurrentSpot;
                  
                  calendarEventDisplay.OtherSpotArray.push( ThisSpot );
                  ThisSpot++;
                  
                  if ( ThisSpot > calendarEventDisplay.CurrentSpot ) 
                  {
                     calendarEventDisplay.CurrentSpot = ThisSpot;
                  } 
               }
            }
            var SortedOtherSpotArray = new Array();
            //the sort must use the global variable gCalendarWindow, not this.calendarWindow
            SortedOtherSpotArray = calendarEventDisplay.OtherSpotArray.sort( gCalendarWindow.compareNumbers);
            var LowestNumber = this.calendarWindow.getLowestElementNotInArray( SortedOtherSpotArray );
            
            //this is the actual spot (0 -> n) that the event will go in on the day view.
            calendarEventDisplay.CurrentSpot = LowestNumber;
            if ( SortedOtherSpotArray.length > 4 ) 
            {
               calendarEventDisplay.NumberOfSameTimeEvents = 4;
            }
            else
            {
               calendarEventDisplay.NumberOfSameTimeEvents = SortedOtherSpotArray.length;
            }
            
         dayEventList[i] = calendarEventDisplay;
         }
   
      }
      var ThisDayAllDayBox = document.getElementById( "all-day-content-box-week-"+dayIndex );
      
      while ( ThisDayAllDayBox.hasChildNodes() ) 
      {
         ThisDayAllDayBox.removeChild( ThisDayAllDayBox.firstChild );
      }
      
      for ( var eventIndex = 0; eventIndex < dayEventList.length; ++eventIndex )
      {
         calendarEventDisplay = dayEventList[ eventIndex ];
   
         // get the day box for the calendarEvent's day
         
         var eventDayInMonth = calendarEventDisplay.event.start.day;
         
         var weekBoxItem = gHeaderDateItemArray[ dayIndex ];
               
         // Display no more than three, show dots for the events > 3
         
         if ( calendarEventDisplay.event.allDay != true ) 
         {
            weekBoxItem.numEvents +=  1;
         }
         
         //if its an all day event, don't show it in the hours bulletin board.
         
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
            AllDayBox.removeAttribute( "collapsed" );
            
            //shrink the day's content box.
            document.getElementById( "week-view-content-box" ).setAttribute( "allday", "true" );
            
            //note the use of the WeekViewAllDayText Attribute.  
            //This is used to remove the text when the day is changed.
            
            newHTMLNode = document.createElement( "label" );
            newHTMLNode.setAttribute( "crop", "end" );
            newHTMLNode.setAttribute( "flex", "1" );            
            newHTMLNode.setAttribute( "value", eventText );
            newHTMLNode.setAttribute( "WeekViewAllDayText", "true" );
            newHTMLNode.calendarEventDisplay = calendarEventDisplay;
            newHTMLNode.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
            newHTMLNode.setAttribute( "onclick", "weekEventItemClick( this, event )" );
            newHTMLNode.setAttribute( "ondblclick", "weekEventItemDoubleClick( this, event )" );
            newHTMLNode.setAttribute( "tooltip", "eventTooltip" );
         
            newImage = document.createElement("image");
				newImage.setAttribute( "class", "all-day-event-class" );
            newImage.setAttribute( "WeekViewAllDayText", "true" );
            newImage.calendarEventDisplay = calendarEventDisplay;
            newImage.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
            newImage.setAttribute( "onclick", "weekEventItemClick( this, event )" );
            newImage.setAttribute( "ondblclick", "weekEventItemDoubleClick( this, event )" );
            newImage.setAttribute( "tooltip", "eventTooltip" );
            
            ThisDayAllDayBox.appendChild( newImage );
            ThisDayAllDayBox.appendChild( newHTMLNode );
         }
         else if ( calendarEventDisplay.CurrentSpot <= 4 ) 
         {
            eventBox = this.createEventBox( calendarEventDisplay, dayIndex );    
            
            //add the box to the bulletin board.
            document.getElementById( "week-view-content-board" ).appendChild( eventBox );
         }

         if( this.calendarWindow.EventSelection.isSelectedEvent( calendarEventDisplay.event ) )
         {
            this.selectBoxForEvent( calendarEventDisplay.event );
         }
      }
         
   }
   //--> END THE FOR LOOP FOR THE WEEK VIEW
}


/** PRIVATE
*
*   This creates an event box for the day view
*/
WeekView.prototype.createEventBox = function ( calendarEventDisplay, dayIndex )
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
   
   var eventBox = document.createElement( "vbox" );
   
   eventBox.calendarEventDisplay = calendarEventDisplay;
   
   var totalWeekWidth = parseFloat(document.defaultView.getComputedStyle(document.getElementById("week-view-content-holder"), "").getPropertyValue("width")) + 1;
   var boxLeftOffset = Math.ceil(parseFloat(document.defaultView.getComputedStyle(document.getElementById("week-tree-hour-0"), "").getPropertyValue("width")));
   var boxWidth = (totalWeekWidth - boxLeftOffset)/ kDaysInWeek;
   var Height = ( hourDuration * kWeekViewHourHeight ) + 1;
   var Width = Math.floor( boxWidth / calendarEventDisplay.NumberOfSameTimeEvents ) + 1;
   eventBox.setAttribute( "height", Height );
   eventBox.setAttribute( "width", Width );
      
   var top = eval( ( startHour*kWeekViewHourHeight ) + ( ( startMinutes/60 ) * kWeekViewHourHeight ) - kWeekViewHourHeightDifference );
   eventBox.setAttribute( "top", top );
   
   var left = eval( boxLeftOffset + ( boxWidth * ( dayIndex - 1 ) ) + ( ( calendarEventDisplay.CurrentSpot - 1 ) * eventBox.getAttribute( "width" ) ) ) ;
   eventBox.setAttribute( "left", left );
   
   eventBox.setAttribute( "class", "week-view-event-class" );
   eventBox.setAttribute( "eventbox", "weekview" );
   eventBox.setAttribute( "onclick", "weekEventItemClick( this, event )" );
   eventBox.setAttribute( "ondblclick", "weekEventItemDoubleClick( this, event )" );
   eventBox.setAttribute( "ondraggesture", "nsDragAndDrop.startDrag(event,calendarViewDNDObserver);" );
   eventBox.setAttribute( "ondragover", "nsDragAndDrop.dragOver(event,calendarViewDNDObserver)" );
   eventBox.setAttribute( "ondragdrop", "nsDragAndDrop.drop(event,calendarViewDNDObserver)" );
   eventBox.setAttribute( "id", "week-view-event-box-"+calendarEventDisplay.event.id );
   eventBox.setAttribute( "name", "week-view-event-box-"+calendarEventDisplay.event.id );
   eventBox.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
   eventBox.setAttribute( "tooltip", "eventTooltip" );
   if( calendarEventDisplay.event.categories && calendarEventDisplay.event.categories != "" )
   {
      eventBox.setAttribute( calendarEventDisplay.event.categories, "true" );
   }      

   /*
   ** The event description. This doesn't go multi line, but does crop properly.
   */
   var eventDescriptionElement = document.createElement( "label" );
   eventDescriptionElement.calendarEventDisplay = calendarEventDisplay;
   eventDescriptionElement.setAttribute( "class", "week-view-event-label-class" );
   eventDescriptionElement.setAttribute( "value", eventText );
   eventDescriptionElement.setAttribute( "flex", "1" );
   var DescriptionText = document.createTextNode( " " );
   eventDescriptionElement.appendChild( DescriptionText );
   eventDescriptionElement.setAttribute( "style", "height: "+Height+";" );
   eventDescriptionElement.setAttribute( "crop", "end" );
   eventDescriptionElement.setAttribute( "ondraggesture", "nsDragAndDrop.startDrag(event,calendarViewDNDObserver);" );
   eventDescriptionElement.setAttribute( "ondragover", "nsDragAndDrop.dragOver(event,calendarViewDNDObserver)" );
   eventDescriptionElement.setAttribute( "ondragdrop", "nsDragAndDrop.drop(event,calendarViewDNDObserver)" );

   eventBox.appendChild( eventDescriptionElement );
   
   
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

   var weekViewButton = document.getElementById( "week_view_command" );
   var monthViewButton = document.getElementById( "month_view_command" );
   var dayViewButton = document.getElementById( "day_view_command" );
   
   monthViewButton.removeAttribute( "disabled" );
   weekViewButton.setAttribute( "disabled", "true" );
   dayViewButton.removeAttribute( "disabled" );

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
   
   // Set the from-to title string, based on the selected date
   var Offset = getIntPref(this.calendarWindow.calendarPreferences.calendarPref, "week.start", 0 );
   
   var viewDay = this.calendarWindow.getSelectedDate().getDay();
   var viewDayOfMonth = this.calendarWindow.getSelectedDate().getDate();
   var viewMonth = this.calendarWindow.getSelectedDate().getMonth();
   var viewYear = this.calendarWindow.getSelectedDate().getFullYear();
   
   viewDay -= Offset;

   if( viewDay < 0 )
      viewDay += 7;

   NewArrayOfDayNames = new Array();
   
   /* 
      Set the header information for the week view
   */
   for( i = 0; i < ArrayOfDayNames.length; i++ )
   {
      NewArrayOfDayNames[i] = ArrayOfDayNames[i];
   }

   for( i = 0; i < Offset; i++ )
   {
      var FirstElement = NewArrayOfDayNames.shift();

      NewArrayOfDayNames.push( FirstElement );
   }

   var dateOfLastDayInWeek = viewDayOfMonth + ( 6 - viewDay );
   
   var dateOfFirstDayInWeek = viewDayOfMonth - viewDay;
   
   var firstDayOfWeek = new Date( viewYear, viewMonth, dateOfFirstDayInWeek );
   var lastDayOfWeek = new Date( viewYear, viewMonth, dateOfLastDayInWeek );
    
   var firstDayMonthName = this.calendarWindow.dateFormater.getMonthName( firstDayOfWeek.getMonth() );
   var lastDayMonthName =  this.calendarWindow.dateFormater.getMonthName( lastDayOfWeek.getMonth() );
   
   var weekNumber = this.getWeekNumber();

   var dateString = "Week "+weekNumber+ ": "+firstDayMonthName + " " + firstDayOfWeek.getDate() + " - " +
                    lastDayMonthName  + " " + lastDayOfWeek.getDate();
   
   var weekTextItem = document.getElementById( "week-title-text" );
   weekTextItem.setAttribute( "value" , dateString ); 
   
   /* done setting the header information */

   /* Fix the day names because users can choose which day the week starts on  */
   var weekDate = new Date( viewYear, viewMonth, dateOfFirstDayInWeek );
   
   for( var dayIndex = 1; dayIndex < 8; ++dayIndex )
   {
      var dateOfDay = weekDate.getDate();
      
      var headerDateItem = document.getElementById( "week-header-date-"+dayIndex );
      headerDateItem.setAttribute( "value" , dateOfDay );
      headerDateItem.setAttribute( "date", weekDate );
      headerDateItem.numEvents = 0;
      
      document.getElementById( "week-header-date-text-"+dayIndex ).setAttribute( "value", NewArrayOfDayNames[dayIndex-1] );
      
      var arrayOfBoxes = new Array();

      if( weekDate.getDay() == 0 || weekDate.getDay() == 6 )
      {
         /* its a weekend */
         arrayOfBoxes = document.getElementsByAttribute( "day", dayIndex );

         for( i = 0; i < arrayOfBoxes.length; i++ )
         {
            arrayOfBoxes[i].setAttribute( "weekend", "true" );
         }
      }
      else
      {
         /* its not a weekend */
         arrayOfBoxes = document.getElementsByAttribute( "day", dayIndex );

         for( i = 0; i < arrayOfBoxes.length; i++ )
         {
            arrayOfBoxes[i].removeAttribute( "weekend" );
         }
      }

      // advance to next day 
      
      weekDate.setDate( dateOfDay + 1 );
   }

   this.hiliteTodaysDate( );
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


WeekView.prototype.selectBoxForEvent = function( calendarEvent )
{
   var EventBoxes = document.getElementsByAttribute( "name", "week-view-event-box-"+calendarEvent.id );
            
   for ( j = 0; j < EventBoxes.length; j++ ) 
   {
      EventBoxes[j].setAttribute( "eventselected", "true" );
   }
}

WeekView.prototype.getVisibleEvent = function( calendarEvent )
{
   eventBox = document.getElementById( "week-view-event-box-"+calendarEvent.id );
   if ( eventBox ) 
   {
      return eventBox;
   }
   else
      return false;

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
      OldTodayArray[i].removeAttribute( "today" );
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
      var ThisDate;
      //today is visible, get the day index for today
      for ( var dayIndex = 1; dayIndex <= 7; ++dayIndex )
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
   //Event = gCalendarWindow.getSelectedEvent();
   
   var ArrayOfBoxes = document.getElementsByAttribute( "eventselected", "true" );

   for( i = 0; i < ArrayOfBoxes.length; i++ )
   {
      ArrayOfBoxes[i].removeAttribute( "eventselected" );   
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
*  Unmark the selected date if there is one.
*/

WeekView.prototype.clearSelectedDate = function( )
{
   if ( this.selectedBox ) 
   {
      this.selectedBox.removeAttribute( "eventselected" );
      this.selectedBox = null;
   }
}


WeekView.prototype.getWeekNumber = function()
{
	var today = new Date( this.calendarWindow.getSelectedDate() );
	var Year = today.getYear() + 1900;
	var Month = today.getMonth();
	var Day = today.getDate();
   var now = new Date(Year,Month,Day+1);
	now = now.getTime();
   var Firstday = new Date( this.calendarWindow.getSelectedDate() );
	Firstday.setYear(Year);
	Firstday.setMonth(0);
	Firstday.setDate(1);
	var then = new Date(Year,0,1);
   then = then.getTime();
	var Compensation = Firstday.getDay();
	if (Compensation > 3) Compensation -= 4;
	else Compensation += 3;
	NumberOfWeek =  Math.round((((now-then)/86400000)+Compensation)/7);
	return NumberOfWeek;
}
