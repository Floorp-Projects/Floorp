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
*   Redraw the events for the current month
* 
*/

DayView.prototype.refreshEvents = function dayview_refreshEvents( )
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
   
   document.getElementById( "day-view-content-box" ).removeAttribute( "allday" );

   //make the text node that will contain the text for the all day box.
   var calendarStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
   
   var HtmlNode = document.createElement( "description" );
   HtmlNode.setAttribute( "class", "all-day-content-box-text-title" );
   var TextNode = document.createTextNode( calendarStringBundle.GetStringFromName( "AllDayEvents" ) );
   HtmlNode.appendChild( TextNode );
   document.getElementById( "all-day-content-box" ).appendChild( HtmlNode );
      
   // get the events for the day and loop through them
   
   var dayEventList = gEventSource.getEventsForDay( this.calendarWindow.getSelectedDate() );
   
   //refresh the array and the current spot.
   var LowestStartHour = getIntPref( this.calendarWindow.calendarPreferences.calendarPref, "event.defaultstarthour", 8 );
   var HighestEndHour = getIntPref( this.calendarWindow.calendarPreferences.calendarPref, "event.defaultendhour", 17 );
   for ( var i = 0; i < dayEventList.length; i++ ) 
   {
      dayEventList[i].OtherSpotArray = new Array('0');
      dayEventList[i].CurrentSpot = 0;
      dayEventList[i].NumberOfSameTimeEvents = 0;
      if( dayEventList[i].event.allDay != true )
      {
         var ThisLowestStartHour = dayEventList[0].displayDate.getHours();
         if( ThisLowestStartHour < LowestStartHour ) 
            LowestStartHour = ThisLowestStartHour;
         
         if( dayEventList[i].event.end.hour > HighestEndHour )
            HighestEndHour = dayEventList[i].event.end.hour;
      }
   }

   for( i = 0; i < 24; i++ )
   {
      document.getElementById( "day-tree-item-"+i ).removeAttribute( "collapsed" );
   }

   //alert( "LowestStartHour is "+LowestStartHour );
   for( i = 0; i < LowestStartHour; i++ )
   {
      document.getElementById( "day-tree-item-"+i ).setAttribute( "collapsed", "true" );
   }
   
   for( i = ( HighestEndHour + 1 ); i < 24; i++ )
   {
      document.getElementById( "day-tree-item-"+i ).setAttribute( "collapsed", "true" );
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
            if ( ( ( thisCalendarEventDisplay.event.displayDate >= calendarEventDisplay.event.displayDate &&
                 thisCalendarEventDisplay.event.displayDate < calendarEventDisplay.event.end.getTime() ) ||
                  ( calendarEventDisplay.displayDate >= thisCalendarEventDisplay.displayDate &&
                 calendarEventDisplay.event.displayDate < thisCalendarEventDisplay.event.end.getTime() ) ) &&
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
         SortedOtherSpotArray = calendarEventDisplay.OtherSpotArray.sort( this.calendarWindow.compareNumbers );
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
         AllDayBox.removeAttribute( "collapsed" );
         
         //shrink the day's content box.
         document.getElementById( "day-view-content-box" ).setAttribute( "allday", "true" );
         
         //note the use of the AllDayText Attribute.  
         //This is used to remove the text when the day is changed.
         
         newTextNode = document.createElement( "label" );
         newTextNode.setAttribute( "value", eventText );
         newTextNode.calendarEventDisplay = calendarEventDisplay;
         newTextNode.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
         newTextNode.setAttribute( "onclick", "dayEventItemClick( this, event )" );
         newTextNode.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
         newTextNode.setAttribute( "tooltip", "eventTooltip" );
         newTextNode.setAttribute( "AllDayText", "true" );
         
         newImage = document.createElement("image");
         newImage.setAttribute( "class", "all-day-event-class" );
         newImage.calendarEventDisplay = calendarEventDisplay;
         newImage.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
         newImage.setAttribute( "onclick", "dayEventItemClick( this, event )" );
         newImage.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
         newImage.setAttribute( "tooltip", "eventTooltip" );
         //newImage.setAttribute( "AllDayText", "true" );
         
         AllDayBox.appendChild( newImage );
         AllDayBox.appendChild( newTextNode );
      }
      else
      {
         eventBox = this.createEventBox( calendarEventDisplay );    
         
         //add the box to the stack.
         document.getElementById( "day-view-content-board" ).appendChild( eventBox );
      }

      // select the hour of the selected item, if there is an item whose date matches
      
      // mark the box as selected, if the event is
            
      if( this.calendarWindow.EventSelection.isSelectedEvent( calendarEventDisplay.event ) )
      {
         this.selectBoxForEvent( calendarEventDisplay.event );
      }
   }
}

/** PRIVATE
*
*   This creates an event box for the day view
*/
DayView.prototype.createEventBox = function dayview_createEventBox( calendarEventDisplay )
{
   var startHour = calendarEventDisplay.displayDate.getHours();
   var startMinutes = calendarEventDisplay.displayDate.getMinutes();

   var eventDuration = ( ( calendarEventDisplay.displayEndDate - calendarEventDisplay.displayDate ) / (60 * 60 * 1000) );
   
   var eventBox = document.createElement( "vbox" );
   
   var boxHeight = document.getElementById( "day-tree-item-"+startHour ).boxObject.height;
   var topHeight = document.getElementById( "day-tree-item-"+startHour ).boxObject.y - document.getElementById( "day-tree-item-"+startHour ).parentNode.boxObject.y;
   var boxWidth = document.getElementById( "day-tree-item-"+startHour ).boxObject.width;
   
   topHeight = eval( topHeight + ( ( startMinutes/60 ) * boxHeight ) );
   topHeight = Math.round( topHeight ) - 2;
   eventBox.setAttribute( "top", topHeight );
   
   eventBox.setAttribute( "height", Math.round( ( eventDuration*boxHeight ) + 1 ) );
   
   var width = Math.round( ( boxWidth-kDayViewHourLeftStart ) / calendarEventDisplay.NumberOfSameTimeEvents );
   eventBox.setAttribute( "width", width );
   
   var left = eval( ( ( calendarEventDisplay.CurrentSpot - 1 ) * width )  + kDayViewHourLeftStart );
   left = left - ( calendarEventDisplay.CurrentSpot - 1 );
   eventBox.setAttribute( "left", Math.round( left ) );
   
   //if you change this class, you have to change calendarViewDNDObserver in calendarDragDrop.js
   eventBox.setAttribute( "class", "day-view-event-class" );
   eventBox.setAttribute( "flex", "1" );
   eventBox.setAttribute( "eventbox", "dayview" );
   eventBox.setAttribute( "onclick", "dayEventItemClick( this, event )" );
   eventBox.setAttribute( "ondblclick", "dayEventItemDoubleClick( this, event )" );
   // dragdrop events
   eventBox.setAttribute( "ondraggesture", "nsDragAndDrop.startDrag(event,calendarViewDNDObserver);" );
   eventBox.setAttribute( "ondragover", "nsDragAndDrop.dragOver(event,calendarViewDNDObserver)" );
   eventBox.setAttribute( "ondragdrop", "nsDragAndDrop.drop(event,calendarViewDNDObserver)" );

   eventBox.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
   eventBox.setAttribute( "tooltip", "eventTooltip" );
   eventBox.setAttribute( "name", "day-view-event-box-"+calendarEventDisplay.event.id );
   if( calendarEventDisplay.event.categories && calendarEventDisplay.event.categories != "" )
   {
      eventBox.setAttribute( calendarEventDisplay.event.categories, "true" );
   }

   var eventHTMLElement = document.createElement( "label" );
   eventHTMLElement.setAttribute( "value", calendarEventDisplay.event.title );
   eventHTMLElement.setAttribute( "flex", "1" );
   eventHTMLElement.setAttribute( "crop", "end" );
   eventBox.appendChild( eventHTMLElement );
   
   /*
   if( calendarEventDisplay.event.description != "" )
   {
      var eventHTMLElement2 = document.createElement( "description" );
      eventHTMLElement2.setAttribute( "value", calendarEventDisplay.event.description );
      eventHTMLElement2.setAttribute( "flex", "1" );
      eventHTMLElement2.setAttribute( "crop", "end" );
      eventBox.appendChild( eventHTMLElement2 );
   }
   */
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
   var monthName = this.calendarWindow.dateFormater.getMonthName( this.calendarWindow.getSelectedDate().getMonth() );
   
   var dateString = monthName + " " + this.calendarWindow.getSelectedDate().getDate() + " " + this.calendarWindow.getSelectedDate().getFullYear();
   
   var dayTextItemPrev2 = document.getElementById( "-2-day-title" );
   var dayTextItemPrev1 = document.getElementById( "-1-day-title" );
   var dayTextItem = document.getElementById( "0-day-title" );
   var dayTextItemNext1 = document.getElementById( "1-day-title" );
   var dayTextItemNext2 = document.getElementById( "2-day-title" );
	var daySpecificTextItem = document.getElementById( "0-day-specific-title" );
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
   
   for ( j = 0; j < EventBoxes.length; j++ ) 
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
   var ArrayOfBoxes = document.getElementsByAttribute( "eventselected", "true" );

   for( i = 0; i < ArrayOfBoxes.length; i++ )
   {
      ArrayOfBoxes[i].removeAttribute( "eventselected" );   
   }
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
