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
*   MultiweekView Class  subclass of CalendarView
*
*  Calendar multiweek view class
*
* PROPERTIES
*     selectedEventBox     - Events are displayed in dynamically created event boxes
*                            this is the selected box, or null
*                            
*     dayNumberItemArray   - An array [ 0 to 41 ] of text boxes that hold the day numbers 
*                            in the current view.
*                            We set the value attribute to the day number.
*                            In the XUL they have id's of the form
*                            multiweek-week-<row_number>-day-<column_number>
*                            where row_number is 1 - 6 and column_number is 1 - 7.
*
*     dayBoxItemArray      - An array [ 0 to 41 ] of  boxes, one for each day in the view. 
*                            These boxes are selected when a day is selected. 
*                            They contain a dayNumberItem and event boxes.
*                            In the XUL they have id's of the form
*                            multiweek-week-<row_number>-day-<column_number>-box
*                            where row_number is 1 - 6 and column_number is 1 - 7.
*
*   firstDateOfView        - A date equal to the date of the first box of the View (the date of 
*                            dayBoxItemArray[0])
*
*   lastDateOfView        - A date equal to the date of the last box of the View (the date of 
*                            dayBoxItemArray[41])
*
*
*    kungFooDeathGripOnEventBoxes - This is to keep the event box javascript objects around so 
*                                   when we get them back they still have the calendar event 
*                                   property on them.
* 
*
*    NOTES
*
*       Events are displayed in dynamically created event boxes. these boxes have a property added
*       to them called "calendarEvent" which contains the event represented by the box. 
*
*       There is one day box item for every day box in the grid. These have an attribute
*       called "empty" which is set to "true", like so: 
*
*                                   dayBoxItem.setAttribute( "empty" , "true" );
*       when the day box is not in the month. This allows the display to be controlled from css.
*
*       There is one dayNumber text box for every day box. These have an attribute called
*       "withmonth" which is set to "true", when the box is the first of a new month. The
*       dayNumber is then completed with a the short month name. The display is controlled
*       from css.
*        
*       The day boxes also have a couple of properties added to them:
*
*            dayBoxItem.numEvents  - The number of events for the day, used to limit the number 
*                                    displayed since there is only room for 3.
* 
*            dayBoxItem.date       - Date of the Box.
* 
*/

var msPerDay = kDate_MillisecondsInDay ;
var msPerMin = kDate_MillisecondsInMinute ;

// Make MultiweekView inherit from CalendarView

MultiweekView.prototype = new CalendarView();
MultiweekView.prototype.constructor = MultiweekView;


/**
*   MultiweekView Constructor.
* 
* PARAMETERS
*      calendarWindow     - the owning instance of CalendarWindow.
*
*/

function MultiweekView( calendarWindow )
{
   // call the super constructor
   
   this.superConstructor( calendarWindow );
   
   this.numberOfEventsToShow = false;

   var multiweekViewEventSelectionObserver = 
   {
      onSelectionChanged : function( EventSelectionArray )
      {
          if( EventSelectionArray.length > 0 )
         {
            setTimeout( "gCalendarWindow.multiweekView.clearSelectedDate();", 1 );

            var i = 0;
            
            for( i = 0; i < EventSelectionArray.length; i++ )
            {
               var EventBoxes;
               if("due" in EventSelectionArray[i] && EventSelectionArray[i].due ) 
		 {
		   EventBoxes = document.getElementsByAttribute( "name", "multiweek-view-todo-box-"+EventSelectionArray[i].id );		   
		 }
	       else
		 {
		   EventBoxes = document.getElementsByAttribute( "name", "multiweek-view-event-box-"+EventSelectionArray[i].id );
		 }
               for (var j = 0; j < EventBoxes.length; j++ ) 
               {
                  EventBoxes[j].setAttribute( "eventselected", "true" );
               }
            }
         }
         else
         {
            //select the proper day
            gCalendarWindow.multiweekView.hiliteSelectedDate();
         }
      }
   }
      
   calendarWindow.EventSelection.addObserver( multiweekViewEventSelectionObserver );
   
   // set up day box's and day number text items, see notes above
   
   this.dayNumberItemArray = new Array();
   this.dayBoxItemArray = new Array();
   this.weekNumberItemArray = new Array();
   this.kungFooDeathGripOnEventBoxes = new Array();
   this.firstDateOfView = new Date();
   this.lastDateOfView = new Date();

   // Get the default number of WeeksInView
   this.localeDefaultsStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
   var defaultWeeksInView = this.localeDefaultsStringBundle.GetStringFromName("defaultWeeksInView" );
   var WeeksInView = getIntPref(this.calendarWindow.calendarPreferences.calendarPref, "weeks.inview", defaultWeeksInView );
   this.WeeksInView = ( WeeksInView >= 6 ) ? 6 : WeeksInView ;

   var dayItemIndex = 0;

    for( var weekIndex = 1; weekIndex <= 6; ++weekIndex )
   {
     var weekNumberItem = document.getElementById( "multiweek-week-" + weekIndex + "-left" );
     this.weekNumberItemArray[ weekIndex ] = weekNumberItem;

       for( var dayIndex = 1; dayIndex <= 7; ++dayIndex )
      {
         // add the day text item to an array[0..41]
         
         var dayNumberItem = document.getElementById( "multiweek-week-" + weekIndex + "-day-" + dayIndex );
         this.dayNumberItemArray[ dayItemIndex ] = dayNumberItem;
         
         // add the day box to an array[0..41]
         
         var dayBoxItem = document.getElementById( "multiweek-week-" + weekIndex + "-day-" + dayIndex + "-box" );
         this.dayBoxItemArray[ dayItemIndex ] = dayBoxItem;
         
         // set on click of day boxes
         
         dayBoxItem.setAttribute( "onclick", "gCalendarWindow.multiweekView.clickDay( event )" );
         dayBoxItem.setAttribute( "oncontextmenu", "gCalendarWindow.multiweekView.contextClickDay( event )" );

         //set the drop
         dayBoxItem.setAttribute( "ondragdrop", "nsDragAndDrop.drop(event,monthViewEventDragAndDropObserver)" );
         dayBoxItem.setAttribute( "ondragover", "nsDragAndDrop.dragOver(event,monthViewEventDragAndDropObserver)" );
         
         //set the double click of day boxes
         dayBoxItem.setAttribute( "ondblclick", "gCalendarWindow.multiweekView.doubleClickDay( event )" );

         // array index
         
         ++dayItemIndex;
      }
   }
}

/** PUBLIC
*
*   Redraw the events and todos for the current view
* 
*   We create XUL boxes dynamically and insert them into the XUL. 
*   To refresh the display we remove all the old boxes and make new ones.
*/
MultiweekView.prototype.refreshEvents = function multiweekView_refreshEvents( )
{
  // Set the numberOfEventsToShow
  if( this.numberOfEventsToShow == false )
      this.setNumberOfEventsToShow();

   // get this view's events and display them
   var viewEventList = gEventSource.getEventsDisplayForRange( this.firstDateOfView,this.lastDateOfView );

   // remove old event boxes
  this.removeElementsByAttribute("eventbox", "multiweekview");
   
   //getAllToDo's
   var viewToDoList = gEventSource.getToDosForRange( this.firstDateOfView,this.lastDateOfView );   

   // remove old todo boxes
  this.removeElementsByAttribute("todobox","multiweekview");

   // clear calendarEvent counts. This controls how many events are shown full, and then the rest are shown as dots.
   // count them by adding a property numEvents to the dayItem, which is zeroed here
   for( var dayItemIndex = 0; dayItemIndex < this.dayBoxItemArray.length; ++dayItemIndex )
   {
      var dayItem = this.dayBoxItemArray[ dayItemIndex ];
      dayItem.numEvents = 0;
   }  
   
   //instead of making a bunch of new date objects, make one here and modify in the for loop
   var DisplayDate = new Date( );
   var eventDayInView;
   var dayBoxItem;
   var calendarToDo;
   //
   // add The ToDo to the view (only due date)
  if(gDisplayToDoInViewChecked === "true" ) {
     for( var todoIndex = 0; todoIndex < viewToDoList.length; ++todoIndex )
       {
	 calendarToDo = viewToDoList[ todoIndex ];
	 DisplayDate.setTime( calendarToDo.due.getTime() );
	 eventDayInView = this.indexOfDate(DisplayDate);
	 
	 dayBoxItem = this.dayBoxItemArray[ eventDayInView ];
	 if( !dayBoxItem )  break;

	 dayBoxItem.numEvents +=  1;
	 // Only displayed if there is enough room
	 if( dayBoxItem.numEvents <= this.numberOfEventsToShow)
	   {
	     var todoBox = this.getToDoBox( calendarToDo,"due" );
	     dayBoxItem.appendChild( todoBox );
	   }
       }
   }
   // add each calendarEvent
   for( var eventIndex = 0; eventIndex < viewEventList.length; ++eventIndex )
   {
      var calendarEventDisplay = viewEventList[ eventIndex ];
      
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
         eventBox.setAttribute( "id", "multiweek-view-event-box-"+calendarEventDisplay.event.id );
         eventBox.setAttribute( "name", "multiweek-view-event-box-"+calendarEventDisplay.event.id );
         //eventBox.setAttribute( "event"+calendarEventDisplay.event.id, true );
         //eventBox.setAttribute( "class", "multiweek-day-event-box-class" );
	 //eventBox.setAttribute( "class", "month-day-event-box-class" );
          
               // start calendar color change by CofC
        var containerName = gCalendarWindow.calendarManager.getCalendarByName(
                                               calendarEventDisplay.event.parent.server ).subject.split(":")[2];

        // set the event box to be of class week-view-event-class and the appropriate calendar-color class
        eventBox.setAttribute("class", "multiweek-day-event-box-class " + containerName );

               // end calendar color change by CofC
                    
         eventBox.setAttribute( "eventbox", "multiweekview" );
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
         eventBoxText.setAttribute( "class", "multiweek-day-event-text-class" );

         if ( calendarEventDisplay.event.allDay == true )
         {
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

         // add the text to the event box and the event box to the day box
         
         eventBox.appendChild( eventBoxText );        
         dayBoxItem.appendChild( eventBox );
      }
      else
      {
         //if there is not a box to hold the little dots for this day...
	     var dotBoxHolder;
         if ( !document.getElementById( "multiweekdotbox"+eventDayInView ) )
         {
            //make one
            dotBoxHolder = document.createElement( "hbox" );
            dotBoxHolder.setAttribute( "id", "multiweekdotbox"+eventDayInView );
            dotBoxHolder.setAttribute( "eventbox", "multiweekview" );
                        
            //add the box to the day.
            dayBoxItem.appendChild( dotBoxHolder );
         }
         else
         {
            //otherwise, get the box
            dotBoxHolder = document.getElementById( "multiweekdotbox"+eventDayInView );
         }
         
         if( dotBoxHolder.childNodes.length < kMAX_NUMBER_OF_DOTS_IN_MONTH_VIEW )
         {
            var eventDotBox = document.createElement( "box" );
            eventDotBox.setAttribute( "eventbox", "multiweekview" );
            
            //show a dot representing an event.
            
            //NOTE: This variable is named eventBox because it needs the same name as 
            // the regular boxes, for the next part of the function!
            eventBox = document.createElement( "image" );
            eventBox.setAttribute( "class", "multiweek-view-event-dot-class" );
            eventBox.setAttribute( "id", "multiweek-view-event-box-"+calendarEventDisplay.event.id );
            eventBox.setAttribute( "name", "multiweek-view-event-box-"+calendarEventDisplay.event.id );
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
*   Create A Box for Todo 
*/

MultiweekView.prototype.getToDoBox = function multiweekView_getToDoBox( calendarToDo,imageprop )
{
  var eventBox = document.createElement( "box" );
  eventBox.setAttribute( "id", "multiweek-view-todo-box-"+calendarToDo.id );
  eventBox.setAttribute( "name", "multiweek-view-todo-box-"+calendarToDo.id );
//  eventBox.setAttribute( "event"+calendarToDo.id, true );
  eventBox.setAttribute( "class", "multiweek-day-event-box-class" );

//   if( calendarToDo.categories && calendarToDo.categories != "" )
//   {
//     eventBox.setAttribute( calendarToDo.categories, "true" );
//   }
            
  eventBox.setAttribute( "todobox", "multiweekview" );
  eventBox.setAttribute( "onclick", "multiweekToDoBoxClickEvent( this, event )" );
  eventBox.setAttribute( "ondblclick", "multiweekToDoBoxDoubleClickEvent( this, event )" );
  eventBox.setAttribute( "onmouseover", "gCalendarWindow.changeMouseOverInfo( calendarToDo, event )" );
  eventBox.setAttribute( "tooltip", "eventTooltip" );
  //eventBox.setAttribute( "ondraggesture", "nsDragAndDrop.startDrag(event,monthViewEventDragAndDropObserver);" );
  // add a property to the event box that holds the calendarEvent that the
  // box represents
  
  eventBox.calendarToDo = calendarToDo;
         
  this.kungFooDeathGripOnEventBoxes.push( eventBox );
  
  // Make a text item to show the event title
  
  var eventBoxText = document.createElement( "label" );
  eventBoxText.setAttribute( "crop", "end" );
  eventBoxText.setAttribute( "class", "multiweek-day-event-text-class" );
  eventBoxText.setAttribute( "value", calendarToDo.title );
  var completed = calendarToDo.completed.getTime();
	  
  if( completed > 0 )
  eventBoxText.setAttribute("completed","true");

  var newImage = document.createElement("image");
  newImage.setAttribute( "class", "todo-due-image-class" );
  if(calendarToDo.priority > 0 && calendarToDo.priority < 5)
     newImage.setAttribute( "highpriority", "true" );
  if(calendarToDo.priority > 5 && calendarToDo.priority < 10)
     newImage.setAttribute( "lowpriority", "true" );
  eventBox.appendChild( newImage );
  eventBoxText.setAttribute( "flex", "1" );

  // add the text to the event box and the event box to the day box
  eventBox.appendChild( eventBoxText );        
  return (eventBox) ; 
}


/** PUBLIC
*
*   Called when the user switches to a different view
*/

MultiweekView.prototype.switchFrom = function multiweekView_switchFrom( )
{
   //Taking care of button
   document.getElementById( "multiweek_view_command" ).removeAttribute( "disabled" );
   //Enable menu options
   document.getElementById( "only-workday-checkbox-1" ).setAttribute( "disabled", "true" );
   document.getElementById( "only-workday-checkbox-2" ).setAttribute( "disabled", "true"  );
   document.getElementById( "display-todo-inview-checkbox-1" ).setAttribute( "disabled", "true"  );
   document.getElementById( "display-todo-inview-checkbox-2" ).setAttribute( "disabled", "true"  );   
   document.getElementById( "menu-numberofweeks-inview" ).setAttribute( "disabled", "true"  );
}


/** PUBLIC
*
*   Called when the user switches to the multiweek view
*/

MultiweekView.prototype.switchTo = function multiweekView_switchTo( )
{
      
   // disable/enable view switching buttons   
   
   var weekViewButton = document.getElementById( "week_view_command" );
   var monthViewButton = document.getElementById( "month_view_command" );
   var dayViewButton = document.getElementById( "day_view_command" );
   
   monthViewButton.removeAttribute( "disabled" );
   weekViewButton.removeAttribute( "disabled" );
   dayViewButton.removeAttribute( "disabled" );

   //Taking care of my button
   document.getElementById( "multiweek_view_command" ).setAttribute( "disabled", "true" );

   //Enable menu options

   document.getElementById( "only-workday-checkbox-1" ).removeAttribute( "disabled" );
   document.getElementById( "only-workday-checkbox-2" ).removeAttribute( "disabled" );
   document.getElementById( "display-todo-inview-checkbox-1" ).removeAttribute( "disabled" );
   document.getElementById( "display-todo-inview-checkbox-2" ).removeAttribute( "disabled" );
   document.getElementById( "menu-numberofweeks-inview" ).removeAttribute( "disabled" );

   // switch views in the deck
   
   var calendarDeckItem = document.getElementById( "calendar-deck" );
   calendarDeckItem.selectedIndex = 3;
}


/** PUBLIC
*
*   Redraw the display, but not the events
*/

MultiweekView.prototype.refreshDisplay = function multiweekView_refreshDisplay( )
{ 
   //set the day names 
   var weekDayOffset = this.preferredWeekStart();
   for(var col = 0; col < 7; col++ )
   {
      document.getElementById( "multiweek-view-header-day-"+(col+1)).value = ArrayOfDayNames[(weekDayOffset + col) % 7];
   }
   
   // set visible weeks
   var weekIndex;
   for( weekIndex = 1 ;  weekIndex <= this.WeeksInView ; ++weekIndex )
   {
     document.getElementById( "multiweek-week-" + weekIndex + "-row" ).removeAttribute( "collapsed" );
   }
   for( weekIndex = this.WeeksInView + 1 ; weekIndex <= 6 ; ++weekIndex )
   {
     document.getElementById( "multiweek-week-" + weekIndex + "-row" ).setAttribute( "collapsed", "true" );
   }

   // read preference for previous weeks in view
   var defaultPreviousWeeksInView = this.localeDefaultsStringBundle.GetStringFromName("defaultPreviousWeeksInView" );
   var PreviousWeeksInView = getIntPref(this.calendarWindow.calendarPreferences.calendarPref, "previousweeks.inview", defaultPreviousWeeksInView );
   this.PreviousWeeksInView = ( PreviousWeeksInView >= this.WeeksInView - 1 ) ? this.WeeksInView - 1 : PreviousWeeksInView ;
   
   var selectedDate = this.calendarWindow.getSelectedDate();
   var newDayOfMonth;
   { 
     // set newDayMonth to first day in prev week (negative ok)
     var prevWeekDayOfMonth = selectedDate.getDate() - 7 * this.PreviousWeeksInView;
     var prevWeekDateCol = (7 - weekDayOffset + selectedDate.getDay()) % 7;
     newDayOfMonth = prevWeekDayOfMonth - prevWeekDateCol;
   }
   var startDate = new Date(selectedDate.getFullYear(), selectedDate.getMonth(), newDayOfMonth);
   this.refreshDisplayFromDate(startDate);
}

/** PRIVATE to multiweek view
 *
 *  Redraw display from given start date (first day of week, may be invisible).
 *  Selected date (this.calendarWindow.getSelectedDate()) must be between
 *  start date (inclusive) and startDate + 7 * weeksInView (exclusive)
 */
MultiweekView.prototype.refreshDisplayFromDate = function multiweekView_refreshDisplayFromDate(startDate) { 
  // figure out first and last days of the month, first and last date of view
  this.firstDateOfView = startDate;
  this.lastDateOfView = new Date( startDate.getFullYear(), startDate.getMonth(), startDate.getDate() + this.WeeksInView*7 - 1, 23, 59, 59 );
  // set the year in the header (top-left)
  document.getElementById( "multiweek-title" ).setAttribute( "value" , startDate.getFullYear() );

  // 'current month' defined by current selectedDate, used to color days in grid
  var selectedDate = this.calendarWindow.getSelectedDate();
  var selectedDateYear = selectedDate.getFullYear();
  var selectedDateMonth = selectedDate.getMonth();
  var firstDayOfMonth  = new Date( selectedDateYear, selectedDateMonth, 1 );
   var firstDayOfMonthIndex = this.indexOfDate( firstDayOfMonth );
  var lastDayOfMonth = DateUtils.getLastDayOfMonth( selectedDateYear, selectedDateMonth );
  var lastDayOfMonthDate  = new Date( selectedDateYear, selectedDateMonth, lastDayOfMonth );
   var lastDayOfMonthIndex = this.indexOfDate( lastDayOfMonthDate );

  var weekDayOffset = this.preferredWeekStart();
  var isOnlyWorkDays = (gOnlyWorkdayChecked == "true");
  var isDayOff = (isOnlyWorkDays? this.preferredDaysOff() : null);
   
  // hide or unhide columns for days off
  for(var day = 0; day < 7; day++) {
    var col = ((7 - weekDayOffset + day) % 7) + 1;
    if( isOnlyWorkDays && isDayOff[day])
      document.getElementById( "multiweek-view-column-"+col ).setAttribute( "collapsed", "true" );
    else 
      document.getElementById( "multiweek-view-column-"+col ).removeAttribute( "collapsed" );
  }

   if( isOnlyWorkDays ) {
     //Is firstDayOfMonth and firstDayOfNextMonth during a weekend ?
     //If so we change the firstDayOfMonthIndex to the first day displayed
     var firstOfMonthWeekDay = firstDayOfMonth.getDay();
     for (var addDays = 0; addDays < 7; addDays++) {
       var weekDay = (7 - weekDayOffset + firstOfMonthWeekDay + addDays) % 7;
       if (!isDayOff[weekDay]) {
	 firstDayOfMonthIndex += addDays;
	 break;
       }
     }
   }


   // To Set Week Number 
   var weekNumberItem;
   var weekNumber ;
   var mondayDate ;
   var mondayOffset = (weekDayOffset >= 5) ? 8 - weekDayOffset : 1 - weekDayOffset ;

   for(var weekIndex = 0; weekIndex < this.WeeksInView; ++weekIndex )
   {
     weekNumberItem = this.weekNumberItemArray[ weekIndex+1 ] ;
     mondayDate = new Date( this.firstDateOfView.getFullYear(), 
			    this.firstDateOfView.getMonth(),
			    this.firstDateOfView.getDate()+mondayOffset+7*weekIndex );

     weekNumber=DateUtils.getWeekNumber(mondayDate);
     weekNumberItem.setAttribute( "value" , weekNumber );  
     //document.getElementById( "multiweek-sb-row" + (weekIndex+1) ).setAttribute( "collapsed","true");
   }
   //document.getElementById( "multiweek-sb-row" + this.WeeksInView).removeAttribute( "collapsed");

   // Write in all the day numbers
   var dayNumberItem ;
   var dayBoxItem ;
   var thisDate = new Date();

   var startYear = startDate.getFullYear();
   var startMonth = startDate.getMonth();
   var startDayOfMonth = startDate.getDate();
   // loop through all the day boxes
   for( var dayIndex = 0; dayIndex < this.dayNumberItemArray.length; ++dayIndex )
   {
     
     dayNumberItem = this.dayNumberItemArray[ dayIndex ];
     dayBoxItem = this.dayBoxItemArray[ dayIndex ];
     thisDate = new Date( startYear, startMonth, startDayOfMonth + dayIndex );

      dayBoxItem.removeAttribute( "empty" ); 
      dayNumberItem.removeAttribute( "withmonth" );

      dayNumberItem.setAttribute( "value" , thisDate.getDate() );
      dayBoxItem.setAttribute( "date", thisDate );
      dayBoxItem.date = thisDate; 

      if( dayIndex < firstDayOfMonthIndex || dayIndex > lastDayOfMonthIndex )
      {
         // this day box is NOT in the month, 
         dayBoxItem.setAttribute( "empty" , "true" );  
         dayBoxItem.removeAttribute( "weekend" );
	 if(dayIndex == lastDayOfMonthIndex + 1) 
	   {
	     titleMonth = this.calendarWindow.dateFormater.getShortMonthName(thisDate.getMonth());
	     this.dayNumberItemArray[ dayIndex ].setAttribute( "value" , thisDate.getDate()+" "+titleMonth );
	     this.dayNumberItemArray[ dayIndex ].setAttribute( "withmonth","true" );
	   }
      }
      else 
	{
	  if(dayIndex == firstDayOfMonthIndex) 
	    {
	      titleMonth = this.calendarWindow.dateFormater.getShortMonthName(selectedDateMonth);
	      this.dayNumberItemArray[ dayIndex ].setAttribute( "value" , thisDate.getDate()+" "+titleMonth );
	      this.dayNumberItemArray[ dayIndex ].setAttribute( "withmonth","true" );
	    }
	  
	  if( thisDate.getDay() == 0 | thisDate.getDay() == 6 )
	    { dayBoxItem.setAttribute( "weekend", "true" ); }
	  else
	    { dayBoxItem.removeAttribute( "weekend" ); }
	}
  
   }

   //Modification for the first day of view
   thisDate = new Date( startYear, startMonth, startDayOfMonth );
   var titleMonth = this.calendarWindow.dateFormater.getShortMonthName(thisDate.getMonth());
   this.dayNumberItemArray[ 0 ].setAttribute( "value" , thisDate.getDate()+" "+titleMonth );
   this.dayNumberItemArray[ 0 ].setAttribute( "withmonth","true" );

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

MultiweekView.prototype.indexOfDate = function multiweekView_indexOfDate(TheDate )
{
  var msOffset = ( this.firstDateOfView.getTimezoneOffset() - TheDate.getTimezoneOffset() ) * msPerMin;
  var IndexInView = Math.floor( ((TheDate.getTime()+msOffset) - this.firstDateOfView.getTime() ) / msPerDay ) ;
  return(IndexInView); 
}

/** PRIVATE
*
*   Mark the selected date, also unmark the old selection if there was one
*/

MultiweekView.prototype.hiliteSelectedDate = function multiweekView_hiliteSelectedDate( )
{
   // Clear the old selection if there was one

   this.clearSelectedDate();

   this.clearSelectedEvent();

   // Set the background for selection
   var indexInView = this.indexOfDate( this.calendarWindow.getSelectedDate() );
   if (indexInView in this.dayBoxItemArray) { 
     var thisBox = this.dayBoxItemArray[ indexInView ];
     if( thisBox )
       thisBox.setAttribute( "multiweekselected" , "true" );
   }
}


/** PUBLIC
*
*  Unmark the selected date if there is one.
*/

MultiweekView.prototype.clearSelectedDate = function multiweekView_clearSelectedDate( )
{
  this.removeAttributeFromElements("multiweekselected", "true");
}


/** PRIVATE
*
*  Mark today as selected, also unmark the old today if there was one.
*/

MultiweekView.prototype.hiliteTodaysDate = function multiweekView_hiliteTodaysDate( )
{
   // Clear the old selection if there was one
  this.removeAttributeFromElements("today", "true");

   //highlight today.
   var Today = new Date( );
   var todayInView = this.indexOfDate( Today );
   //var todayInView = Math.floor( (Today.getTime() - this.firstDateOfView ) / msPerDay ) ;

   if ( todayInView >= 0 && todayInView <    this.WeeksInView * 7 ) 
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

MultiweekView.prototype.getNewEventDate = function multiweekView_getNewEventDate()
{
   // use the selected year, month and day
   // and the current hours and minutes
   
   var start = this.calendarWindow.getSelectedDate();
   
   var now = new Date();
   start.setHours( now.getHours() );
   start.setMinutes( Math.ceil( now.getMinutes() / 30 ) * 30 );
   start.setSeconds( 0 );
   
   return start;     
}


/** PUBLIC
*
*   Moves goWeeks weeks in the future, goes to next month if no argument.
*   Negative goWeeks moves to weeks in the past.
*/

MultiweekView.prototype.goToNext = function multiweekView_goToNext( goWeeks )
{  
  if (! goWeeks) goWeeks = 1;
  var startDate = new Date(this.firstDateOfView.getFullYear(), this.firstDateOfView.getMonth(), this.firstDateOfView.getDate() + 7 * goWeeks);
  var selectedDate = this.calendarWindow.getSelectedDate();
  var nextPageStartDate = new Date(startDate.getFullYear(), startDate.getMonth(), startDate.getDate() + 7 * this.WeeksInView);
  if (startDate <= selectedDate && selectedDate < nextPageStartDate) {
    // old selected date is still in view, keep selection
    this.refreshDisplayFromDate(startDate);
    this.refreshEvents();
  } else { 
    // old selected date is not in view, so select same day in new focus week.
    var weekStartDay = this.preferredWeekStart();
    var daysAfterWeekStart = (7 + selectedDate.getDay() - weekStartDay) % 7;
    var offsetDays = 7 * this.PreviousWeeksInView + daysAfterWeekStart;
    this.calendarWindow.goToDay(new Date(startDate.getFullYear(), startDate.getMonth(), startDate.getDate() + offsetDays));
   }
}
MultiweekView.prototype.goToNextPage = function multiweekView_goToNextPage( )
{
  this.goToNext( this.WeeksInView )
}

/** PUBLIC
*
*   Goes goWeeks weeks into the past, goes to the previous month if no argument.
*/

MultiweekView.prototype.goToPrevious = function multiweekView_goToPrevious( goWeeks )
{
  this.goToNext( goWeeks? -goWeeks : -1 );
}
MultiweekView.prototype.goToPreviousPage = function multiweekView_goToPreviousPage( )
{
  this.goToPrevious( this.WeeksInView )
}


/** PUBLIC  -- multiweekview only
*
*   Called when a day box item is single clicked and on first click of double click
*/

MultiweekView.prototype.clickDay = function multiweekView_clickDay( event )
{
   if( event.button > 0 )
      return;
  
   if ( event.detail == 1 ) { // first click
   var dayBoxItem = event.currentTarget;
      // change the selected date and redraw it
      this.calendarWindow.setSelectedDate( dayBoxItem.date );

     //changing the selection will redraw the day as selected (colored blue) in the multiweek view.
     //therefore, this has to happen after setSelectedDate
      gCalendarWindow.EventSelection.emptySelection();
   }
}

/** PUBLIC  -- multiweekview only
*
*   Called when a day box item is single clicked
*/
MultiweekView.prototype.contextClickDay = function multiweekView_contextClickDay( event )
{
   if( event.currentTarget.date )
   {
      // set the date for newEventCommand without selecting date
      gNewDateVariable = event.currentTarget.date;
   }
}

/*
** Calls newEvent dialog on 2nd click of double click.
   clickDay will have been called on the first click, so day will be selected.
*/

MultiweekView.prototype.doubleClickDay = function multiweekView_doubleClickDay( event )
{
   if( event.button > 0 )
      return;
   
      newEvent( this.getNewEventDate(), false );
}


MultiweekView.prototype.clearSelectedEvent = function multiweekView_clearSelectedEvent( )
{
  debug("clearSelectedEvent");

  this.removeAttributeFromElements("eventselected", "true");
}


MultiweekView.prototype.getVisibleEvent = function multiweekView_getVisibleEvent( calendarEvent )
{
   var eventBox = document.getElementById( "multiweek-view-event-box-"+calendarEvent.id );

   if ( eventBox ) 
   {
      return eventBox;
   }
   else
      return false;
}

MultiweekView.prototype.selectBoxForEvent = function multiweekView_selectBoxForEvent( calendarEvent )
{
   var EventBoxes = document.getElementsByAttribute( "name", "multiweek-view-event-box-"+calendarEvent.id );
            
   for (var j = 0; j < EventBoxes.length; j++ ) 
   {
      EventBoxes[j].setAttribute( "eventselected", "true" );
   }
}

/*Just calls setCalendarSize, it's here so it can be implemented on the other two views without difficulty.*/
MultiweekView.prototype.doResize = function multiweekView_doResize( )
{
   this.setCalendarSize(this.getViewHeight());

   this.setNumberOfEventsToShow();
}

/*Takes in a height, sets the calendar's container box to that height, the grid expands and contracts to fit it.*/
MultiweekView.prototype.setCalendarSize = function multiweekView_setCalendarSize( height )
{
    height = height+"px";
    document.getElementById( "multiweek-content-box" ).setAttribute( "height", height );
} 

/*returns the height of the current view in pixels*/ 
MultiweekView.prototype.getViewHeight = function multiweekView_getViewHeight( )
{
    var toReturn = document.defaultView.getComputedStyle(document.getElementById("multiweek-view-box"), "").getPropertyValue("height");
    toReturn = parseInt( toReturn ); //strip off the px at the end
    return toReturn;
}

MultiweekView.prototype.setNumberOfEventsToShow = function multiweekView_setNumberOfEventsToShow( )
{
  this.setFictitiousEvents() ;

   //get the style height of the month view box.
   var MultiweekViewBoxHeight = document.defaultView.getComputedStyle(this.dayBoxItemArray[ 3 ],"").getPropertyValue("height");
   MultiweekViewBoxHeight = parseFloat( MultiweekViewBoxHeight ); //strip off the px at the end
   
   //get the style height of the label of the month view box.
   var MultiweekViewLabelHeight = document.defaultView.getComputedStyle(document.getElementById("multiweek-week-1-day-4"), "").getPropertyValue("height");
   MultiweekViewLabelHeight = parseFloat( MultiweekViewLabelHeight ); //strip off the px at the end
   
   //get the height of an event box.
   var eventBox = document.getElementById( "multiweek-view-event-box-fictitious" );
   //get the height of the event dot box holder.
   var dotBox = document.getElementById( "multiweekdotbox-fictitious" );
  if( !dotBox || !eventBox) 
    return;

  var EventBoxHeight = document.defaultView.getComputedStyle( eventBox, "" ).getPropertyValue( "height" );
  EventBoxHeight = parseFloat( EventBoxHeight ); //strip off the px at the end
  // BUG ? : the following computation doesnot return the height of dotBoxHolder as expected
  // Could someone has a good idea why ?
  // To overcome, I put a value of 11px for the Height
  // I had also a margin of 3px
  var EventDotBoxHeight = document.defaultView.getComputedStyle( dotBox, "" ).getPropertyValue( "height" );
  EventDotBoxHeight = parseFloat( EventDotBoxHeight ); //strip off the px at the end
  
   //calculate the number of events to show.
  var numberOfEventsToShow = parseInt(  ( MultiweekViewBoxHeight - MultiweekViewLabelHeight - 14) / EventBoxHeight )
  this.numberOfEventsToShow = numberOfEventsToShow ;

  // remove created event boxes
  var Element = document.getElementById( "multiweek-view-event-box-fictitious-dot") ;
  Element.parentNode.removeChild(Element);
  eventBox.parentNode.removeChild( eventBox );
  dotBox.parentNode.removeChild( dotBox );
}

/* Change the number of weeks in the view */
MultiweekView.prototype.changeNumberOfWeeks = function multiweekView_changeNumberOfWeeks(el) 
{
  var v=el.getAttribute("value") ;
  this.WeeksInView = parseInt(v) ;
  this.refreshDisplay() ;
}

/* Draw a event and a dotboxholder for evaluation of the sizes */

MultiweekView.prototype.setFictitiousEvents = function multiweekView_setFictitiousEvents( )
{
  var dayBoxItem = this.dayBoxItemArray[ 3 ];
  if( !dayBoxItem ) 
     return;
  // Make a box item to hold the event
  var eventBox = document.createElement( "box" );
  eventBox.setAttribute( "id", "multiweek-view-event-box-fictitious" );
  eventBox.setAttribute( "class", "multiweek-day-event-box-class" );
  eventBox.setAttribute( "eventbox", "multiweekview" );
  dayBoxItem.appendChild( eventBox );
  // Make a text item to show the event title
  var eventBoxText = document.createElement( "label" );
  eventBoxText.setAttribute( "crop", "end" );
  eventBoxText.setAttribute( "class", "multiweek-day-event-text-class" );
  // To format the starting time of the event display as "12:15 titleevent"
  eventBoxText.setAttribute( "value",  "12:15 fictitious event");
  //you need this flex in order for text to crop
  eventBoxText.setAttribute( "flex", "1" );
  // add the text to the event box and the event box to the day box
  eventBox.appendChild( eventBoxText );        
  //make one dot box holder
  var dotBoxHolder = document.createElement( "hbox" );
  dotBoxHolder.setAttribute( "id", "multiweekdotbox-fictitious" );
  dotBoxHolder.setAttribute( "dotboxholder", "multiweekview" );
  //add the box to the day.
  dayBoxItem.appendChild( dotBoxHolder );
  //make the dot box
  var eventDotBox = document.createElement( "box" );
  eventDotBox.setAttribute( "eventdotbox", "multiweekview" );          
  eventBox = document.createElement( "image" );
  eventBox.setAttribute( "class", "multiweek-view-event-dot-class" );
  eventBox.setAttribute( "id", "multiweek-view-event-box-fictitious-dot" );
  eventDotBox.appendChild( eventBox );
  dotBoxHolder.appendChild( eventDotBox );
}


function debug( Text )
{
   dump( "\nmultiweekView.js: "+ Text);

}
