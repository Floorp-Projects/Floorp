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
 *                 Karl Guertin <grayrest@grayrest.com> 
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
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
*                            We set the value attribute to the day number.
*                            In the XUL they have id's of the form  month-week-<row_number>-day-<column_number>
*                            where row_number is 1 - 6 and column_number is 1 - 7.
*
*     dayBoxItemArray      - An array [ 0 to 41 ] of  boxes, one for each day in the month view. These boxes
*                            are selected when a day is selected. They contain a dayNumberItem and event boxes.
*                            In the XUL they have id's of the form  month-week-<row_number>-day-<column_number>-box
*                            where row_number is 1 - 6 and column_number is 1 - 7.
*
*   firstDateOfView        - A date equal to the date of the first box of the View (the date of 
*                            dayBoxItemArray[0])
*
*   lastDateOfView        - A date equal to the date of the last box of the View (the date of 
*                            dayBoxItemArray[41])
*
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
*            dayBoxItem.date       - Date of the Box.
* 
*/

var msPerDay = kDate_MillisecondsInDay ;
var msPerMin = kDate_MillisecondsInMinute ;
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
   
   this.numberOfEventsToShow = false;

   var monthViewEventSelectionObserver = 
   {
      onSelectionChanged : function( EventSelectionArray )
      {
         if( EventSelectionArray.length > 0 )
         {
            //if there are selected events.
            
            //for some reason, this function causes the tree to go into a select / unselect loop
            //putting it in a settimeout fixes this.
            setTimeout( "gCalendarWindow.monthView.clearSelectedDate();", 1 );

            gCalendarWindow.monthView.clearSelectedBoxes();
            
            var i = 0;
            
            for( i = 0; i < EventSelectionArray.length; i++ )
            {
               var EventBoxes = document.getElementsByAttribute( "name", "month-view-event-box-"+EventSelectionArray[i].id );
               for ( var j = 0; j < EventBoxes.length; j++ ) 
               {
                  EventBoxes[j].setAttribute( "eventselected", "true" );
               }
            }
         }
         else
         {
            //select the proper day
            gCalendarWindow.monthView.hiliteSelectedDate();
         }
      }
   }
      
   calendarWindow.EventSelection.addObserver( monthViewEventSelectionObserver );
   
   this.showingLastDay = false;
  
   // set up month day box's and day number text items, see notes above
   
   this.dayNumberItemArray = new Array();
   this.dayBoxItemArray = new Array();
   this.weekNumberItemArray = new Array();
   this.kungFooDeathGripOnEventBoxes = new Array();
   this.firstDateOfView = new Date();
   this.lastDateOfView = new Date();

   var dayItemIndex = 0;
   
   for( var weekIndex = 1; weekIndex <= 6; ++weekIndex )
   {
     var weekNumberItem = document.getElementById( "month-week-" + weekIndex + "-left" );
     this.weekNumberItemArray[ weekIndex ] = weekNumberItem;

      for( var dayIndex = 1; dayIndex <= 7; ++dayIndex )
      {
         // add the day text item to an array[0..41]
         
         var dayNumberItem = document.getElementById( "month-week-" + weekIndex + "-day-" + dayIndex );
         this.dayNumberItemArray[ dayItemIndex ] = dayNumberItem;
         
         // add the day box to an array[0..41]
         
         var dayBoxItem = document.getElementById( "month-week-" + weekIndex + "-day-" + dayIndex + "-box" );
         this.dayBoxItemArray[ dayItemIndex ] = dayBoxItem;
         
         // set on click of day boxes
         
         dayBoxItem.setAttribute( "onclick", "gCalendarWindow.monthView.clickDay( event )" );
         dayBoxItem.setAttribute( "oncontextmenu", "gCalendarWindow.monthView.contextClickDay( event )" );

         //set the drop
         dayBoxItem.setAttribute( "ondragdrop", "nsDragAndDrop.drop(event,monthViewEventDragAndDropObserver)" );
         dayBoxItem.setAttribute( "ondragover", "nsDragAndDrop.dragOver(event,monthViewEventDragAndDropObserver)" );
         
         //set the double click of day boxes
         dayBoxItem.setAttribute( "ondblclick", "gCalendarWindow.monthView.doubleClickDay( event )" );

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
MonthView.prototype.refreshEvents = function monthView_refreshEvents( )
{
  // Set the numberOfEventsToShow
  if( this.numberOfEventsToShow == false )
         this.setNumberOfEventsToShow();

   // get this month's events and display them
   var monthEventList = gEventSource.getEventsDisplayForRange( this.firstDateOfView,this.lastDateOfView );
   
   // remove old event boxes
  this.removeElementsByAttribute("eventbox", "monthview");
   
   // clear calendarEvent counts. This controls how many events are shown full, and then the rest are shown as dots.

   // count them by adding a property numEvents to the dayItem, which is zeroed here
   for( var dayItemIndex = 0; dayItemIndex < this.dayBoxItemArray.length; ++dayItemIndex )
   {
      var dayItem = this.dayBoxItemArray[ dayItemIndex ];
      dayItem.numEvents = 0;
   }  
   this.kungFooDeathGripOnEventBoxes = new Array();
   
   //instead of making a bunch of new date objects, make one here and modify in the for loop
   var DisplayDate = new Date( );
   var eventDayInView;
   var dayBoxItem;
   var calIndex;
   var calNumber;

   // add each calendarEvent
   for( var eventIndex = 0; eventIndex < monthEventList.length; ++eventIndex )
   {
      var calendarEventDisplay = monthEventList[ eventIndex ];
      
      // get the day box for the calendarEvent's day
      DisplayDate.setTime( calendarEventDisplay.displayDate );
      eventDayInView = this.indexOfDate(DisplayDate);
      dayBoxItem = this.dayBoxItemArray[ eventDayInView ];

      if( !dayBoxItem )
         break;

      // Display no more than three, show dots for the events > this.numberOfEventsToShow
      
      dayBoxItem.numEvents +=  1;
      
      var eventBox;
      if( dayBoxItem.numEvents <= this.numberOfEventsToShow )
      {
         // Make a box item to hold the event
         eventBox = document.createElement( "box" );
         eventBox.setAttribute( "id", "month-view-event-box-"+calendarEventDisplay.event.id );
         eventBox.setAttribute( "name", "month-view-event-box-"+calendarEventDisplay.event.id );
         //eventBox.setAttribute( "event"+toString(calendarEventDisplay.event.id), true );
	   
		// start calendar color change by CofC
        var containerName = gCalendarWindow.calendarManager.getCalendarByName(
						calendarEventDisplay.event.parent.server ).subject.split(":")[2];

        // set the event box to be of class week-view-event-class and the appropriate calendar-color class
        eventBox.setAttribute("class", "month-day-event-box-class " + containerName );

		// end calendar color change by CofC

         eventBox.setAttribute( "eventbox", "monthview" );
         eventBox.setAttribute( "onclick", "monthEventBoxClickEvent( this, event )" );
         eventBox.setAttribute( "ondblclick", "monthEventBoxDoubleClickEvent( this, event )" );
         eventBox.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
         eventBox.setAttribute( "tooltip", "eventTooltip" );
         eventBox.setAttribute( "ondraggesture", "nsDragAndDrop.startDrag(event,monthViewEventDragAndDropObserver);" );
         // add a property to the event box that holds the calendarEvent that the
         // box represents

         eventBox.calendarEventDisplay = calendarEventDisplay;
         
         this.kungFooDeathGripOnEventBoxes.push( eventBox );
         
         // Make a text item to show the event title
         
         var eventBoxText = document.createElement( "label" );
         eventBoxText.setAttribute( "crop", "end" );
         eventBoxText.setAttribute( "class", "month-day-event-text-class" );
 
         if ( calendarEventDisplay.event.allDay == true )
         {
            eventBox.setAttribute( "allday", "true" );
            eventBoxText.setAttribute( "value", calendarEventDisplay.event.title );
            // Create an image
            var newImage = document.createElement("image");
            newImage.setAttribute( "class", "all-day-event-class" );
            eventBox.appendChild( newImage );
         }
         else
         {
            // To format the starting time of the event
            var eventStartTime = new Date( calendarEventDisplay.event.start.getTime() ) ;
            var StartFormattedTime = this.calendarWindow.dateFormater.getFormatedTime( eventStartTime );
            // display as "12:15 titleevent"
            eventBoxText.setAttribute( "value",  StartFormattedTime+' '+calendarEventDisplay.event.title);
         }

         //you need this flex in order for text to crop
         eventBoxText.setAttribute( "flex", "1" );
         eventBoxText.setAttribute( "ondraggesture", "nsDragAndDrop.startDrag(event,monthViewEventDragAndDropObserver);" );

         // add the text to the event box and the event box to the day box
         
         eventBox.appendChild( eventBoxText );        
         
         dayBoxItem.appendChild( eventBox );
      }
      else
      {
         //if there is not a box to hold the little dots for this day...
         var dotBoxHolder;
         if ( !document.getElementById( "dotboxholder"+eventDayInView ) )
         {
            //make one
            dotBoxHolder = document.createElement( "hbox" );
            dotBoxHolder.setAttribute( "id", "dotboxholder"+eventDayInView );
            dotBoxHolder.setAttribute( "eventbox", "monthview" );
                        
            //add the box to the day.
            dayBoxItem.appendChild( dotBoxHolder );
         }
         else
         {
            //otherwise, get the box
            dotBoxHolder = document.getElementById( "dotboxholder"+eventDayInView );
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
            eventBox.setAttribute( "id", "month-view-event-box-"+calendarEventDisplay.event.id );
            eventBox.setAttribute( "name", "month-view-event-box-"+calendarEventDisplay.event.id );
            eventBox.calendarEventDisplay = calendarEventDisplay;
            eventBox.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarEventDisplay, event )" );
            eventBox.setAttribute( "onclick", "monthEventBoxClickEvent( this, event )" );
            eventBox.setAttribute( "ondblclick", "monthEventBoxDoubleClickEvent( this, event )" );
            eventBox.setAttribute( "tooltip", "eventTooltip" );
   
            this.kungFooDeathGripOnEventBoxes.push( eventBox );
            
            //add the dot to the extra box.
            eventDotBox.appendChild( eventBox );
            dotBoxHolder.appendChild( eventDotBox );
         }
      }

      // mark the box as selected, if the event is
      if( this.calendarWindow.EventSelection.isSelectedEvent( calendarEventDisplay.event ) )
      {
         this.selectBoxForEvent( calendarEventDisplay.event ); 
      } 
   }
}


/** PUBLIC
*
*   Called when the user switches to a different view
*/

MonthView.prototype.switchFrom = function monthView_switchFrom( )
{
   document.getElementById( "only-workday-checkbox-1" ).setAttribute( "disabled", "true" );
   document.getElementById( "only-workday-checkbox-2" ).setAttribute( "disabled", "true"  );
}


/** PUBLIC
*
*   Called when the user switches to the month view
*/

MonthView.prototype.switchTo = function monthView_switchTo( )
{
   // see showingLastDay notes above
   
   this.showingLastDay = false;
      
   // disable/enable view switching buttons   
   
   var weekViewButton = document.getElementById( "week_view_command" );
   var monthViewButton = document.getElementById( "month_view_command" );
   var dayViewButton = document.getElementById( "day_view_command" );
   
   monthViewButton.setAttribute( "disabled", "true" );
   weekViewButton.removeAttribute( "disabled" );
   dayViewButton.removeAttribute( "disabled" );

   //Enable menu options

   document.getElementById( "only-workday-checkbox-1" ).removeAttribute( "disabled" );
   document.getElementById( "only-workday-checkbox-2" ).removeAttribute( "disabled" );

   // switch views in the deck
   
   var calendarDeckItem = document.getElementById( "calendar-deck" );
   calendarDeckItem.selectedIndex = 0;
}


/** PUBLIC
*
*   Redraw the display, but not the events
*/

MonthView.prototype.refreshDisplay = function monthView_refreshDisplay( )
{ 
   // set the month/year in the header
   
   var selectedDate = this.calendarWindow.getSelectedDate();
   var newMonth = selectedDate.getMonth();
   var newYear =  selectedDate.getFullYear();
   var titleMonthArray = new Array();
   var titleYearArray = new Array();
   var toDebug = "";
   for (var i=-2; i < 3; i++){
      titleMonthArray[i] = newMonth + i;
      titleMonthArray[i] = (titleMonthArray[i] >= 0)? titleMonthArray[i] % 12 : titleMonthArray[i] + 12;
      titleMonthArray[i] = this.calendarWindow.dateFormater.getMonthName( titleMonthArray[i] );
      var idName = "m"+ i + "-month-title";
      document.getElementById( idName ).setAttribute( "value" , titleMonthArray[i] );
   }
   
   document.getElementById( "m0-month-title" ).setAttribute( "value" , titleMonthArray[0] + " " + newYear );
   
  var Offset = this.preferredWeekStart();
  var isOnlyWorkDays = (gOnlyWorkdayChecked == "true");
  var isDayOff = (isOnlyWorkDays? this.preferredDaysOff() : null);

   var NewArrayOfDayNames = new Array();
   
   for( i = 0; i < ArrayOfDayNames.length; i++ )
   {
      NewArrayOfDayNames[i] = ArrayOfDayNames[i];
   }

   for( i = 0; i < Offset; i++ )
   {
      var FirstElement = NewArrayOfDayNames.shift();

      NewArrayOfDayNames.push( FirstElement );
   }

   //set the day names 
   for( i = 1; i <= 7; i++ )
   {
      document.getElementById( "month-view-header-day-"+i ).value = NewArrayOfDayNames[ (i-1) ];
      document.getElementById( "month-view-column-"+i ).removeAttribute( "collapsed" );
   }
   

   // Write in all the day numbers
   
   // figure out first and last days of the month, first and last date of view
   
   var firstDate = new Date( newYear, newMonth, 1 );
   var firstDateCol = (7 - Offset + firstDate.getDay()) % 7;

   var lastDayOfMonth = DateUtils.getLastDayOfMonth( newYear, newMonth );
   var lastDateCol = (firstDateCol + lastDayOfMonth - 1) % 7;

   this.firstDateOfView = new Date( newYear, newMonth, 1 - firstDateCol, 0, 0, 0 );
   this.lastDateOfView = new Date( newYear, newMonth,  42 - firstDateCol, 23, 59, 59 );
   
  // hide or unhide columns for days off
  for(var day = 0; day < 7; day++) {
    var dayCol = ((7 - Offset + day) % 7) + 1;
    if( isOnlyWorkDays && isDayOff[day])
      document.getElementById( "month-view-column-"+dayCol ).setAttribute( "collapsed", "true" );
    else 
      document.getElementById( "month-view-column-"+dayCol ).removeAttribute( "collapsed" );
  }

  { // first week should not display if holds only off days and only work days displayed
    var isFirstWeekDisplayed = true;
    if (isOnlyWorkDays) {
      isFirstWeekDisplayed = false;
      for (var col = firstDateCol; col < 7; col++) {
	if (!isDayOff[(Offset + col) % 7]) {
	  isFirstWeekDisplayed = true;
	  break;
	}
      }
    }
    if (!isFirstWeekDisplayed) {
      document.getElementById( "month-week-1-row" ).setAttribute( "collapsed", "true" );
    } else {
      document.getElementById( "month-week-1-row" ).removeAttribute( "collapsed" );
    }
  }

  { // sixth week should not display if holds only off days and only work days displayed
    var isSixthWeekDisplayed = (firstDateCol + lastDayOfMonth > 5 * 7);
    if (isSixthWeekDisplayed && isOnlyWorkDays) {
      isSixthWeekDisplayed = false;
      for (var sixthWeekCol = 0; sixthWeekCol <= lastDateCol; sixthWeekCol++) {
	if (!isDayOff[(Offset + sixthWeekCol) % 7]) {
	  isSixthWeekDisplayed = true;
	  break;
	}
      }
    }
    if(!isSixthWeekDisplayed) {
      document.getElementById( "month-week-6-row" ).setAttribute( "collapsed", "true" );
    } else {
      document.getElementById( "month-week-6-row" ).removeAttribute( "collapsed" );
    }

    // fifth week should not display if holds only off days and only work days displayed
    var isFifthWeekDisplayed = (firstDateCol + lastDayOfMonth > 4 * 7);
    if (isFifthWeekDisplayed && !isSixthWeekDisplayed && isOnlyWorkDays) {
      isFifthWeekDisplayed = false;
      for (var fifthWeekCol = 0; fifthWeekCol <= lastDateCol; fifthWeekCol++) {
	if (!isDayOff[(Offset + fifthWeekCol) % 7]) {
	  isFifthWeekDisplayed = true;
	  break;
	}
      }
    }
    if(!isFifthWeekDisplayed) {
      document.getElementById( "month-week-5-row" ).setAttribute( "collapsed", "true" );
    } else {
      document.getElementById( "month-week-5-row" ).removeAttribute( "collapsed" );
    }
  }


  // To Set Week Number 
  var weekNumberItem;
  var weekNumber ;
  var mondayDate ;
  var newoffset = (Offset >= 5) ? 8 -Offset : 1 - Offset ;
  for( var weekIndex = 0; weekIndex < 6; ++weekIndex )
  {
    weekNumberItem = this.weekNumberItemArray[ weekIndex+1 ] ;
    mondayDate = new Date( this.firstDateOfView.getFullYear(), 
			    this.firstDateOfView.getMonth(),
			    this.firstDateOfView.getDate()+newoffset+7*weekIndex );

    weekNumber=DateUtils.getWeekNumber(mondayDate);
    weekNumberItem.setAttribute( "value" , weekNumber );  
  }
   
   // loop through all the day boxes
   
   var dayNumber = 1;

   for( var dayIndex = 0; dayIndex < this.dayNumberItemArray.length; ++dayIndex )
   {
      var dayNumberItem = this.dayNumberItemArray[ dayIndex ];
      var dayBoxItem = this.dayBoxItemArray[ dayIndex ];
      var thisDate = new Date( newYear, newMonth, 1-(firstDateCol - dayIndex ) );

      dayBoxItem.date = thisDate;

      dayNumberItem.setAttribute( "value" , thisDate.getDate() );  

      if( dayIndex < firstDateCol || dayNumber > lastDayOfMonth )
      {
         // this day box is NOT in the month, 
         dayBoxItem.dayNumber = null;

         dayBoxItem.setAttribute( "empty" , "true" );  
         dayBoxItem.removeAttribute( "weekend" );
      }  
      else
      {
         dayBoxItem.removeAttribute( "empty" ); 

         if( thisDate.getDay() == 0 | thisDate.getDay() == 6 )
         {
            dayBoxItem.setAttribute( "weekend", "true" );
         }
         else
            dayBoxItem.removeAttribute( "weekend" ); 

         dayBoxItem.dayNumber = dayNumber;
         
         ++dayNumber;  
      }
   }
  
   // if we aren't showing an event, highlite the selected date.
   if ( this.calendarWindow.EventSelection.selectedEvents.length < 1 ) 
   {
      this.hiliteSelectedDate( );
   }
   
   //always highlight today's date.
   this.hiliteTodaysDate( );
}

/** PRIVATE
*
*   Return the Index in the view of the given Date
*/

MonthView.prototype.indexOfDate = function monthView_indexOfDate(TheDate )
{
  var msOffset = ( this.firstDateOfView.getTimezoneOffset() - TheDate.getTimezoneOffset() ) * msPerMin;
  var IndexInView = Math.floor( ((TheDate.getTime()+msOffset) - this.firstDateOfView.getTime() ) / msPerDay ) ;
  return(IndexInView); 
}

/** PRIVATE
*
*   Mark the selected date, also unmark the old selection if there was one
*/

MonthView.prototype.hiliteSelectedDate = function monthView_hiliteSelectedDate( )
{
   // Clear the old selection if there was one

   this.clearSelectedDate();

   this.clearSelectedBoxes();

   // Set the background for selection
   var indexInView = this.indexOfDate(this.calendarWindow.getSelectedDate());
   if (indexInView in this.dayBoxItemArray) { 
     var thisBox = this.dayBoxItemArray[ indexInView ];
     if( thisBox )
       thisBox.setAttribute( "monthselected" , "true" );
   }
}


/** PUBLIC
*
*  Unmark the selected date if there is one.
*/

MonthView.prototype.clearSelectedDate = function monthView_clearSelectedDate( )
{
  this.removeAttributeFromElements("monthselected","true");
}

/** PUBLIC
*
*  Unmark the selected date if there is one.
*/

MonthView.prototype.clearSelectedBoxes = function monthView_clearSelectedBoxes( )
{
  this.removeAttributeFromElements("eventselected","true");
}

/** PRIVATE
*
*  Mark today as selected, also unmark the old today if there was one.
*/

MonthView.prototype.hiliteTodaysDate = function monthView_hiliteTodaysDate( )
{
   // Clear the old selection if there was one
  this.removeAttributeFromElements("today","true");

   //highlight today.
   var Today = new Date( );
   var todayInView = this.indexOfDate(Today) ;

   if ( todayInView >= 0 && todayInView < 42 ) 
   {
      var ThisBox = this.dayBoxItemArray[ todayInView ];
      if( ThisBox )
         ThisBox.setAttribute( "today", "true" );
   }
}


/** PUBLIC
*
*   This is called when we are about the make a new event
*   and we want to know what the default start date should be for the event.
*/

MonthView.prototype.getNewEventDate = function monthView_getNewEventDate( )
{
   // use the selected year, month and day
   // and the current hours and minutes
   
   return combineWithCurrentTime( this.calendarWindow.getSelectedDate() );
}
   
/* PRIVATE
   
   Create new datetime with date from date and
   time near current time (mod 5 min).**/
function combineWithCurrentTime( date ) {
  var newDateTime = new Date(date); // copy date, don't modify old date
  var now = new Date();
  newDateTime.setHours(now.getHours());
  newDateTime.setMinutes( Math.ceil( now.getMinutes() / 5 ) * 5 );
  newDateTime.setSeconds( 0 );
  return newDateTime;
}

/** PUBLIC
*
*   Moves goMonths months in the future, goes to next month if no argument.
*/

MonthView.prototype.goToNext = function monthView_goToNext( goMonths )
{  
   var nextMonth;

   if(goMonths){
      nextMonth = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth() + goMonths, 1 );
      this.adjustNewMonth( nextMonth );  
      this.goToDay( nextMonth );
   }else{
      nextMonth = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth() + 1, 1 );
      this.adjustNewMonth( nextMonth );  
      this.goToDay( nextMonth );
   }
}


/** PUBLIC
*
*   Goes goMonths months into the past, goes to the previous month if no argument.
*/

MonthView.prototype.goToPrevious = function monthView_goToPrevious( goMonths )
{
   var prevMonth;

   if(goMonths){
      prevMonth = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth() - goMonths, 1 );
      this.adjustNewMonth( prevMonth );  
      this.goToDay( prevMonth );
   }else{
      prevMonth = new Date(  this.calendarWindow.selectedDate.getFullYear(),  this.calendarWindow.selectedDate.getMonth() - 1, 1 );
      this.adjustNewMonth( prevMonth );  
      this.goToDay( prevMonth );
   }
   
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

MonthView.prototype.adjustNewMonth = function monthView_adjustNewMonth( newMonth )
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

MonthView.prototype.clickDay = function monthView_clickDay( event )
{
   if( event.button > 0 )
      return;
  
   var dayBoxItem = event.currentTarget;
   
   if( dayBoxItem.dayNumber != null && event.detail == 1 )
   {
      // turn off showingLastDay - see notes in MonthView class
      this.showingLastDay = false;
   
      // change the selected date and redraw it
      this.calendarWindow.setSelectedDate( dayBoxItem.date );

      //changing the selection will redraw the day as selected (colored blue) in the month view.
      //therefor, this has to happen after setSelectedDate
      gCalendarWindow.EventSelection.emptySelection();
   }
}

/** PUBLIC  -- monthview only
*
*   Called when a day box item is single clicked
*/
MonthView.prototype.contextClickDay = function monthView_contextClickDay( event )
{
   if( event.currentTarget.dayNumber != null )
   {
      // turn off showingLastDay - see notes in MonthView class
      
      this.showingLastDay = false;
   
      // change the selected date and redraw it
      
      gNewDateVariable = gCalendarWindow.getSelectedDate();

      gNewDateVariable.setDate( event.currentTarget.dayNumber );
   }
}

/*
** Don't forget that clickDay gets called before double click day gets called
*/

MonthView.prototype.doubleClickDay = function monthView_doubleClickDay( event )
{
   if( event.button > 0 )
      return;
   
   if ( event.currentTarget.dayNumber != null ) 
   {
      // change the selected date and redraw it

      newEvent( this.getNewEventDate(), false );

   }
   else
   {
     newEvent( combineWithCurrentTime( event.currentTarget.date ), false );
   }
}


MonthView.prototype.clearSelectedEvent = function monthView_clearSelectedEvent( )
{
  debug("clearSelectedEvent");
  this.removeAttributeFromElements("eventselected","true");
}


MonthView.prototype.getVisibleEvent = function monthView_getVisibleEvent( calendarEvent )
{
   var eventBox = document.getElementById( "month-view-event-box-"+calendarEvent.id );

   if ( eventBox ) 
   {
      return eventBox;
   }
   else
      return false;
}

MonthView.prototype.selectBoxForEvent = function monthView_selectBoxForEvent( calendarEvent )
{
   var EventBoxes = document.getElementsByAttribute( "name", "month-view-event-box-"+calendarEvent.id );
            
   for ( j = 0; j < EventBoxes.length; j++ ) 
   {
      EventBoxes[j].setAttribute( "eventselected", "true" );
   }
}

/*Just calls setCalendarSize, it's here so it can be implemented on the other two views without difficulty.*/
MonthView.prototype.doResize = function monthView_doResize( )
{
   this.setCalendarSize(this.getViewHeight());

   this.setNumberOfEventsToShow();
}

/*Takes in a height, sets the calendar's container box to that height, the grid expands and contracts to fit it.*/
MonthView.prototype.setCalendarSize = function monthView_setCalendarSize( height )
{
    var offset = document.defaultView.getComputedStyle(document.getElementById("month-controls-box"), "").getPropertyValue("height");
    offset = parseInt( offset );
    height = (height-offset)+"px";
    document.getElementById( "month-content-box" ).setAttribute( "height", height );
} 

/*returns the height of the current view in pixels*/ 
MonthView.prototype.getViewHeight = function monthView_getViewHeight( )
{
    var toReturn = document.defaultView.getComputedStyle(document.getElementById("month-view-box"), "").getPropertyValue("height");
    toReturn = parseInt( toReturn ); //strip off the px at the end
    return toReturn;
}


MonthView.prototype.setNumberOfEventsToShow = function monthView_setNumberOfEventsToShow( )
{
  this.setFictitiousEvents() ;

   //get the style height of the month view box.
   var MonthViewBoxHeight = document.defaultView.getComputedStyle(this.dayBoxItemArray[24],"").getPropertyValue("height");
   MonthViewBoxHeight = parseFloat( MonthViewBoxHeight ); //strip off the px at the end
   
   //get the style height of the label of the month view box.
   var MonthViewLabelHeight = document.defaultView.getComputedStyle(document.getElementById("month-week-4-day-4"), "").getPropertyValue("height");
   MonthViewLabelHeight = parseFloat( MonthViewLabelHeight ); //strip off the px at the end
   
   //get the height of an event box.
   var eventBox = document.getElementById( "month-view-event-box-fictitious" );
   //get the height of the event dot box holder.
   var dotBox = document.getElementById( "dotboxholder-fictitious" );
  if( !dotBox || !eventBox) 
      return;

  var EventBoxHeight = document.defaultView.getComputedStyle( eventBox, "" ).getPropertyValue( "height" );
  EventBoxHeight = parseFloat( EventBoxHeight ); //strip off the px at the end
  // BUG ? : the following computation doesnot return the height of dotBoxHolder has expected
  // Could someone has a good idea why ?
  // To overcome, I put a value of 11px for the Height
  // I had also a margin of 3px
  var EventDotBoxHeight = document.defaultView.getComputedStyle( dotBox, "" ).getPropertyValue( "height" );
  EventDotBoxHeight = parseFloat( EventDotBoxHeight ); //strip off the px at the end

   //calculate the number of events to show.
  var numberOfEventsToShow = parseInt(  ( MonthViewBoxHeight - MonthViewLabelHeight - 14) / EventBoxHeight )
  this.numberOfEventsToShow = numberOfEventsToShow ;

  // remove created event boxes
  var Element = document.getElementById( "month-view-event-box-fictitious-dot") ;
  Element.parentNode.removeChild(Element);
  eventBox.parentNode.removeChild( eventBox );
  dotBox.parentNode.removeChild( dotBox );
}


/* Draw a event and a dotboxholder for evaluation of the sizes */

MonthView.prototype.setFictitiousEvents = function monthView_setFictitiousEvents( )
{
  var dayBoxItem = this.dayBoxItemArray[ 24 ];
  if( !dayBoxItem ) 
     return;
  // Make a box item to hold the event
  var eventBox = document.createElement( "box" );
  eventBox.setAttribute( "id", "month-view-event-box-fictitious" );
  eventBox.setAttribute( "class", "month-day-event-box-class" );
  eventBox.setAttribute( "eventbox", "monthview" );
  // Make a text item to show the event title
  var eventBoxText = document.createElement( "label" );
  eventBoxText.setAttribute( "crop", "end" );
  eventBoxText.setAttribute( "class", "month-day-event-text-class" );
  // To format the starting time of the event display as "12:15 titleevent"
  eventBoxText.setAttribute( "value",  "12:15 fictitious event");
  //you need this flex in order for text to crop
  eventBoxText.setAttribute( "flex", "1" );
  // add the text to the event box and the event box to the day box
  eventBox.appendChild( eventBoxText );        
  dayBoxItem.appendChild( eventBox );
  //make one dot box holder
  var dotBoxHolder = document.createElement( "hbox" );
  dotBoxHolder.setAttribute( "id", "dotboxholder-fictitious" );
  dotBoxHolder.setAttribute( "dotboxholder", "monthview" );
  //add the box to the day.
  dayBoxItem.appendChild( dotBoxHolder );
  //make the dot box
  var eventDotBox = document.createElement( "box" );
  eventDotBox.setAttribute( "eventdotbox", "monthview" );          
  eventBox = document.createElement( "image" );
  eventBox.setAttribute( "class", "month-view-event-dot-class" );
  eventBox.setAttribute( "id", "month-view-event-box-fictitious-dot" );
  eventDotBox.appendChild( eventBox );
  dotBoxHolder.appendChild( eventDotBox );
}


/*
drag and drop stuff 
*/
var gEventBeingDragged = false;
var gBoxBeingDroppedOn = false;

var monthViewEventDragAndDropObserver  = {
  onDragStart: function (evt, transferData, action){
      if( evt.target.calendarEventDisplay ) 
         gEventBeingDragged = evt.target.calendarEventDisplay.event;
      else {
	if( evt.target.parentNode.calendarEventDisplay )
	  gEventBeingDragged = evt.target.parentNode.calendarEventDisplay.event;
	else if(evt.target.calendarToDo || evt.target.parentNode.calendarToDo) {
	  dump("\n ToDo Drag not supported yet");
	  gEventBeingDragged = null ;
	  return;
	}
      }
      transferData.data=new TransferData();
      transferData.data.addDataForFlavour("text/unicode",0);
  },
  getSupportedFlavours : function () {
    var weekflavours = new FlavourSet();
    weekflavours.appendFlavour("text/unicode");
    return weekflavours;
  },
  onDragOver: function (evt,flavour,session){
    gBoxBeingDroppedOn = document.getElementById( evt.target.getAttribute( "id" ) );
  },
  onDrop: function (evt,dropdata,session){
    //get the date of the current event box.
    if( gBoxBeingDroppedOn.date == null )
        return;
    
    var newDay = new Date(gBoxBeingDroppedOn.date) ;

    var oldStartDate = new Date( gEventBeingDragged.start.getTime() );
    var oldEndDate = new Date( gEventBeingDragged.end.getTime() );

    var newStartDate = new Date( oldStartDate.getTime() ) ;
    newStartDate.setFullYear( newDay.getFullYear());
    //call setMonth() twice necessary, because of a bug ? observed in Mozilla 1.2.1. :
    //     - the bug consequence is that an event drag from month 0 (Jan.) to month 1 (Feb)
    //       is set by a single call to month 2 (Mar). 
    newStartDate.setMonth( newDay.getMonth() );
    newStartDate.setMonth( newDay.getMonth() );
    newStartDate.setDate( newDay.getDate());

    var Difference = newStartDate.getTime() - oldStartDate.getTime();
    
    var newEndDate = oldEndDate.getTime() + Difference;

    //shift the days from their old days to their new days.
    //Coupled with the shift below : Does this work ?
    if( Difference < 0 )
        Difference += 7;

    //edit the event being dragged to change its start and end date
    //don't change the start and end time though.
    if( evt.ctrlKey == true )
    {
        var eventToCopy = gEventBeingDragged.clone();
        eventToCopy.id = null; //set the id to null so that it generates a new one
        eventToCopy.start.setTime( newStartDate.getTime() );
        eventToCopy.end.setTime( newEndDate );
    
        if( eventToCopy.recurWeekdays > 0 )
          {
            eventToCopy.recurWeekdays = eventToCopy.recurWeekdays << Difference;
          }
        
        // LINAGORA: Needed to update remote calendar
        addEventDialogResponse( eventToCopy, gEventBeingDragged.parent.server );
        //gICalLib.addEvent( eventToCopy, gEventBeingDragged.parent.server );  
    }
    else
    {
      gEventBeingDragged.start.setTime( newStartDate.getTime() );
      gEventBeingDragged.end.setTime( newEndDate );
    
      if( gEventBeingDragged.recurWeekdays > 0 )
      {
        gEventBeingDragged.recurWeekdays = gEventBeingDragged.recurWeekdays << Difference;
      }
      // LINAGORA: Needed to update remote calendar
      modifyEventDialogResponse( gEventBeingDragged, gEventBeingDragged.parent.server );
      //gICalLib.modifyEvent( gEventBeingDragged, gEventBeingDragged.parent.server );
    }
  }
};

function debug( Text )
{
   dump( "\nmonthView.js: "+ Text);

}

