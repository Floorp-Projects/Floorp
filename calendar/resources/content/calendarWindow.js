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
*  C A L E N D A R     C L A S S E S
*/




/*-----------------------------------------------------------------
*   CalendarWindow Class
*
*  Maintains the three calendar views and the selection.
*
* PROPERTIES
*     eventSource            - the event source, an instance of CalendarEventDataSource
*     dateFormater           - a date formatter, an instance of DateFormater
*   
*     monthView              - an instance of MonthView
*     weekView               - an instance of WeekView
*     dayView                - an instance of DayView
*     multiweekView         - an instance of MultiweekView
*   
*     currentView            - the currently active view, one of the above three instances 
*     
*     selectedEvent          - the selected event, instance of CalendarEvent
*     selectedDate           - selected date, instance of Date 
*  
*/


/**
*   CalendarWindow Constructor.
* 
* PARAMETERS
*      calendarDataSource     - The data source with all of the calendar events.
*
* NOTES
*   There is one instance of CalendarWindow
*/

function CalendarWindow( )
{
   //setup the preferences
   this.calendarPreferences = new calendarPreferences( this );

   // miniMonth used by preferences
   this.miniMonth = document.getElementById( "lefthandcalendar" );
   
   //setup the calendars
   this.calendarManager = new calendarManager( this );
   
   //setup the calendar event selection
   this.EventSelection = new CalendarEventSelection( this );

   //global date formater
   this.dateFormater = new DateFormater( this );
   
   //the different views for the calendar
   this.monthView = new MonthView( this );
   this.weekView = new WeekView( this );
   this.dayView = new DayView( this );
   this.multiweekView = new MultiweekView( this );
   
   // we keep track of the selected date and the selected
   // event, start with selecting today.
   
   this.selectedEvent = null;
   this.selectedDate = new Date();
   
   // set up the current view - we assume that this.currentView is NEVER null
   // after this
   
   this.currentView = null;   
   
   //depending on the selected index, change views to that view.
   var SelectedIndex = document.getElementById( "calendar-deck" ).selectedIndex;
   
   switch( SelectedIndex )
   {
      case "1":
         this.currentView = this.weekView;
         document.getElementById( "week-tree-hour-0" ).focus();
         break;
      case "2":
         this.currentView = this.dayView;
         document.getElementById( "day-tree-item-0" ).focus();
         break;
      case "3":
         this.currentView = this.multiweekView;
         break;
      default:
         this.currentView = this.monthView;
         break;
   }
      
   // now that everything is set up, we can start to observe the data source
   
   // make the observer, the calendarEventDataSource calls into the
   // observer when things change in the data source.
   
   var calendarWindow = this;
   
   this.calendarEventDataSourceObserver =
   {
      
      onLoad   : function()
      {
         //this is basically useless, since the calendar window is not yet made yet.
      },
      onStartBatch   : function()
      {
      },
      onEndBatch   : function()
      {
         calendarWindow.currentView.refreshEvents( );
      },
      
      onAddItem : function( calendarEvent )
      {
         if( !gICalLib.batchMode )
         {
             if( calendarEvent )
             {
                calendarWindow.setSelectedEvent( calendarEvent );

                calendarWindow.currentView.refreshEvents( );
             }
         }
      },
   
      onModifyItem : function( calendarEvent, originalEvent )
      {
        if( !gICalLib.batchMode )
        {
             calendarWindow.currentView.refreshEvents( );
        }
      },
   
      onDeleteItem : function( calendarEvent, nextEvent )
      {
         //if you put this outside of the batch mode, deleting all events
         //puts the calendar into an infinite loop.
           
         if( !gICalLib.batchMode )
         {
            calendarWindow.currentView.refreshEvents( );
            
            if ( nextEvent ) 
            {
               calendarWindow.setSelectedEvent( nextEvent );
            }
            else
            {
               //get the tree, see if there are items to highlight.

               var today = new Date( );

               //calendarWindow.setSelectedDate( getNextOrPreviousRecurrence( calendarEvent ) );
               calendarWindow.setSelectedDate( today );

               if( "hiliteSelectedDate" in calendarWindow.currentView )
                  calendarWindow.currentView.hiliteSelectedDate( );
            }
         }
      },
      onAlarm : function( calendarEvent )
      {
      
      },
      onError : function()
      {
      }
   };
   
   // add the observer to the event source
   
   gICalLib.addObserver( this.calendarEventDataSourceObserver );
}


/** PUBLIC
*
*   You must call this when you have finished with the CalendarWindow.
*   Removes the observer from the data source.
*/

CalendarWindow.prototype.close = function calWin_close( )
{
   gICalLib.removeObserver(  this.calendarEventDataSourceObserver ); 
}


/** PUBLIC
*
*   Switch to the day view if it isn't already the current view
*/

CalendarWindow.prototype.switchToDayView = function calWin_switchToDayView( )
{
   document.getElementById( "day-tree-item-0" ).focus();
   
   this.switchToView( this.dayView )
}


/** PUBLIC
*
*   Switch to the week view if it isn't already the current view
*/

CalendarWindow.prototype.switchToWeekView = function calWin_switchToWeekView( )
{
   document.getElementById( "week-tree-hour-0" ).focus();

   this.switchToView( this.weekView )
}


/** PUBLIC
*
*   Switch to the month view if it isn't already the current view
*/

CalendarWindow.prototype.switchToMonthView = function calWin_switchToMonthView( )
{
   this.switchToView( this.monthView )
}

/** PUBLIC
*
*   Switch to the multiweek view if it isn't already the current view
*/

CalendarWindow.prototype.switchToMultiweekView = function calWin_switchToMultiweekView( )
{
   this.switchToView( this.multiweekView )
}

/** PUBLIC
*
*   Display today in the current view
*/

CalendarWindow.prototype.goToToday = function calWin_goToToday( )
{
   this.clearSelectedEvent( );

   var Today = new Date();

   this.currentView.goToDay( Today );

   document.getElementById( "lefthandcalendar" ).value = Today;

}
/** PUBLIC
*
*   Choose a date, then go to that date in the current view.
*/

CalendarWindow.prototype.pickAndGoToDate = function calWin_pickAndGoToDate( )
{
  var currentView = this.currentView;
  var args = new Object();
  args.initialDate = this.getSelectedDate();  
  args.onOk = function receiveAndGoToDate( pickedDate ) {
    currentView.goToDay( pickedDate );
    document.getElementById( "lefthandcalendar" ).value = pickedDate;
  };
  openDialog("chrome://calendar/content/goToDateDialog.xul",
             "GoToDateDialog", // target= window name
             "chrome,modal", args);
}

/** PUBLIC
*
*   Go to the next period in the current view
*/

CalendarWindow.prototype.goToNext = function calWin_goToNext( value )
{
   if(value){
      this.currentView.goToNext( value );
   }else{
      this.currentView.goToNext();
   }
}


/** PUBLIC
*
*   Go to the previous period in the current view
*/

CalendarWindow.prototype.goToPrevious = function calWin_goToPrevious( value )
{   
   if(value){
      this.currentView.goToPrevious( value );
   }else{
      this.currentView.goToPrevious( );
   }
}


/** PUBLIC
*
*   Go to today in the current view
*/

CalendarWindow.prototype.goToDay = function calWin_goToDay( newDate )
{
   this.currentView.goToDay( newDate );
}


/** PACKAGE
*
*   Change the selected event
*
* PARAMETERS
*   selectedEvent  - an instance of CalendarEvent, MUST be from the CalendarEventDataSource
*
*/

CalendarWindow.prototype.setSelectedEvent = function calWin_setSelectedEvent( selectedEvent )
{
   this.EventSelection.replaceSelection( selectedEvent );
}


/** PACKAGE
*
*   Clear the selected event
*
* PARAMETERS
*   unSelectedEvent  - if null:            deselect the selected event.
*                    - if a CalendarEvent: only deselect if it is the currently selected one.
*
*/

CalendarWindow.prototype.clearSelectedEvent = function calWin_clearSelectedEvent( unSelectedEvent )
{
   var undefined;
   
   this.EventSelection.emptySelection( );
      
   if( unSelectedEvent === undefined || unSelectedEvent == null )
   {
      this.currentView.clearSelectedEvent( );
   }
}


/** PUBLIC
*
*   Set the selected date
*/

CalendarWindow.prototype.setSelectedDate = function calWin_setSelectedDate( date, noHighlight )
{
   // Copy the date because we might mess with it in place
   
   this.selectedDate = new Date( date );

   /* on some machines, we need to check for .selectedItem first */
   if( document.getElementById( "event-filter-menulist" ) && 
       document.getElementById( "event-filter-menulist" ).selectedItem &&
       document.getElementById( "event-filter-menulist" ).selectedItem.value == "current" )
   {
      //redraw the top tree
      setTimeout( "refreshEventTree( getAndSetEventTable() );", 150 );
   }
   
   if( "hiliteSelectedDate" in this.currentView && noHighlight != false )
      this.currentView.hiliteSelectedDate( );
}

/** PUBLIC
*
*   Get the selected date
*/

CalendarWindow.prototype.getSelectedDate = function calWin_getSelectedDate( )
{
   // Copy the date because we might mess with it in place
   
   return new Date( this.selectedDate );
}


/** PUBLIC
*
*   Change the hour of the selected date
*/

CalendarWindow.prototype.setSelectedHour = function calWin_setSelectedHour( hour )
{
   var selectedDate = this.getSelectedDate();
   
   selectedDate.setHours( hour );
   selectedDate.setMinutes( 0 );
   selectedDate.setSeconds( 0 );
   
   this.setSelectedDate( selectedDate );
}

/** PRIVATE
*
*   Helper function to switch to a view
*
* PARAMETERS
*   newView  - MUST be one of the three CalendarView instances created in the constructor
*/

CalendarWindow.prototype.switchToView = function calWin_switchToView( newView )
{
   // only switch if not already there
   
   if( this.currentView !== newView )
   {
      // call switch from for the view we are leaving
      
      if( this.currentView )
      {
         this.currentView.switchFrom();
      }
      
      // change the current view
      
      this.currentView = newView;
      
      // switch to and refresh the view
      
      newView.switchTo();
      
      newView.refresh();
   }
}


/** PUBLIC
*
*  This changes the text for the popuptooltip text
*  This is the same for any view.
*/

CalendarWindow.prototype.changeMouseOverInfo = function calWin_changeMouseOverInfo( calendarEvent, event )
{
   var Html = document.getElementById( "eventTooltip" );

   while( Html.hasChildNodes() )
   {
      Html.removeChild( Html.firstChild ); 
   }

   var HolderBox;
   if("calendarToDo" in event.currentTarget) 
   {
      HolderBox = getPreviewTextForTask( event.currentTarget.calendarToDo );
   }
   else
   {
      HolderBox = getPreviewTextForRepeatingEvent( event.currentTarget.calendarEventDisplay );
   }
   
   Html.appendChild( HolderBox );
}

/** PRIVATE
*
*   This returns the lowest element not in the array
*  eg. array(0, 0, 1, 3) would return 2
*   Used to figure out where to put the day events.
*
*/

CalendarWindow.prototype.getLowestElementNotInArray = function calWin_getLowestElementNotInArray( InputArray )
{
   var Temp = 1;
   var AllZero = true; //are all the elements in the array 0?
   //CAUTION: Watch the scope here.  This function is called from inside a nested for loop.
   //You can't have the index variable named anything that is used in those for loops.

   for ( var Mike = 0; Mike < InputArray.length; Mike++ ) 
   {
      
      if ( InputArray[Mike] > Temp ) 
      {
         return (Temp);
      }
      if ( InputArray[Mike] > 0)
      {
         AllZero = false;
         Temp++; //don't increment if the array value is 0, otherwise add 1.
      }
   }
   if ( AllZero ) 
   {
      return (1);
   }
   return (Temp);
}


/** PRIVATE
*
*   Use these comparison functions in Array.sort
*
*/

CalendarWindow.prototype.compareNumbers = function calWin_compareNumbers(a, b) {
   return a - b
}

CalendarWindow.prototype.compareDisplayEventStart = function calWin_compareDisplayEventStart(a, b) {
   return ( a.displayDate - b.displayDate );
}

CalendarWindow.prototype.compareDisplayEventEnd = function calWin_compareDisplayEventStart(a, b) {
   return ( a.displayEndDate - b.displayEndDate );
}

CalendarWindow.prototype.onMouseUpCalendarSplitter = function calWinOnMouseUpCalendarSplitter()
{
   //check if calendar-splitter is collapsed
   this.doResize();
}

CalendarWindow.prototype.onMouseUpCalendarViewSplitter = function calWinOnMouseUpCalendarViewSplitter()
{
   //check if calendar-view-splitter is collapsed
   if( document.getElementById( "bottom-events-box" ).getAttribute( "collapsed" ) != "true" )
   {
      //do this because if they started with it collapsed, its not showing anything right now.

      //in a setTimeout to give the pull down menu time to draw.
      setTimeout( "refreshEventTree( getAndSetEventTable() );", 10 );
   }
   
   this.doResize();
}

/** PUBLIC
*   The resize handler, used to set the size of the views so they fit the screen.
*/
 window.onresize = CalendarWindow.prototype.doResize = function calWin_doResize(){
     gCalendarWindow.currentView.refresh();
}




/*-----------------------------------------------------------------
*   CalendarView Class
*
*  Abstract super class for the three view classes
*
* PROPERTIES
*     calendarWindow    - instance of CalendarWindow that owns this view
*  
*/

function CalendarView( calendarWindow )
{
   this.calendarWindow = calendarWindow;
}

/** A way for subclasses to invoke the super constructor */

CalendarView.prototype.superConstructor = CalendarView;

/** PUBLIC
*
*   Select the date and show it in the view
*   Params: newDate: the new date to go to.
*   ShowEvent: Do we show an event being highlighted, or do we show a day being highlighted.
*/

CalendarView.prototype.goToDay = function calView_goToDay( newDate, ShowEvent )
{
   var oldDate = this.calendarWindow.getSelectedDate();
   
   this.calendarWindow.setSelectedDate( newDate ); 
   
   switch( this.calendarWindow.currentView )
   {
      case this.calendarWindow.monthView:
         if( newDate.getFullYear() != oldDate.getFullYear() ||
             newDate.getMonth() != oldDate.getMonth() )
         {
            this.refresh( ShowEvent )
         }
         break;
   
      case this.calendarWindow.multiweekView:
      case this.calendarWindow.weekView:
      case this.calendarWindow.dayView:
         if( newDate.getFullYear() != oldDate.getFullYear() ||
             newDate.getMonth() != oldDate.getMonth() ||
             newDate.getDate() != oldDate.getDate() )
         {
            this.refresh( ShowEvent )
         }
         break;
   }
}

/** PUBLIC
*
*   Refresh display of events and the selection in the view
*/

CalendarView.prototype.refresh = function calView_refresh( ShowEvent )
{
   this.refreshDisplay( ShowEvent )
   
   if("doResize" in this.calendarWindow.currentView)
      this.calendarWindow.currentView.doResize();
   
   this.refreshEvents()
}

/** PRIVATE
 * 
 * Calculate startHour (the first hour of 'date' that has events, 
 *                      or default value from prefs) and  
 *           endHour (the last hour of 'date' that has events,
 *                    or default value from prefs).
 */

CalendarView.prototype.getViewLimits = function calView_getViewLimits( dayDisplayEventList, date )
{
  //set defaults from preferences
  var sHour = getIntPref( this.calendarWindow.calendarPreferences.calendarPref, "event.defaultstarthour", 8 );
  var eHour = getIntPref( this.calendarWindow.calendarPreferences.calendarPref, "event.defaultendhour", 17 );

  //get date start and end as millisecs for comparisons
  var tmpDate =new Date(date);
  tmpDate.setHours(0,0,0);
  var dateStart = tmpDate.valueOf();
  tmpDate.setHours(23,59,59);
  var dateEnd = tmpDate.valueOf();
  
  for ( var i = 0; i < dayDisplayEventList.length; i++ ) {
    if( dayDisplayEventList[i].event.allDay != true ) {
      
      if( dayDisplayEventList[i].displayDate < dateStart ) {
        sHour=0;
      } else {
        if( dayDisplayEventList[i].displayDate <= (dateStart + sHour*kDate_MillisecondsInHour)) 
          sHour = Math.floor((dayDisplayEventList[i].displayDate - dateStart)/kDate_MillisecondsInHour);
      }
      
      if( dayDisplayEventList[i].displayEndDate > dateEnd ) {
        eHour=23;
      } else {
        if( dayDisplayEventList[i].displayEndDate > (dateStart + (eHour)*kDate_MillisecondsInHour) ) 
          eHour = Math.ceil((dayDisplayEventList[i].displayEndDate - dateStart)/kDate_MillisecondsInHour);
      }
    }
  }
  return { startHour: sHour, endHour: eHour };
}

/** PRIVATE
*   Sets the following for all events in dayEventList 
*      event.startDrawSlot  - the first horizontal slot the event occupies
*      event.drawSlotCount  - how many slots the event occupies
*      event.totalSlotCount - total horizontal slots (during  events duration)
*   Used in DayView, WeekView
*/
CalendarView.prototype.setDrawProperties = function calView_setDrawProperties( dayEventList ) {
  //non-allday Events sorted on displayDate
  var dayEventStartList = new Array();
  var eventStartListIndex = 0;

  //non-allday Events sorted on displayEndDate
  var dayEventEndList = new Array();
  var eventEndListIndex = 0;
  
  // parallel events
  var currEventSlots = new Array();

  // start index of the current (contiguous) group of events.
  var groupStartIndex = 0;

  var nextEventIsStarting = true;
  var currEventSlotsIsEmpty
  var done = false;

  var i;
  for( i = 0; i < dayEventList.length; i++ )
    if( !dayEventList[i].event.allDay) {
      dayEventStartList.push(dayEventList[i]);
      dayEventEndList.push(dayEventList[i]);
    }
    
  if( dayEventStartList.length > 0 ) {

    dayEventStartList.sort(this.calendarWindow.compareDisplayEventStart);
    dayEventEndList.sort(this.calendarWindow.compareDisplayEventEnd);
    
    //init (horizontal) event draw slots .
    for ( i = 0; i < dayEventStartList.length; i++ ) 
    {
      dayEventStartList[i].startDrawSlot  = -1;
      dayEventStartList[i].drawSlotCount  = -1;
      dayEventStartList[i].totalSlotCount = -1;
    }
    
    while( !done ) {
      if( nextEventIsStarting ) { 
        
        //find a slot for the event
        for( i = 0; i <currEventSlots.length; i++ )
          if( currEventSlots[i] == null ) {            
            // found empty slot, set event here
            // also check if there is room for event wider than 1 slot.
            var k = i;
            dayEventStartList[eventStartListIndex].startDrawSlot = i;
            while( ( k < currEventSlots.length ) && ( currEventSlots[k] == null )) {
              currEventSlots[k] = dayEventStartList[eventStartListIndex];
              dayEventStartList[eventStartListIndex].drawSlotCount = k - i + 1;
              k++;
            }
            break;
          }
        if( dayEventStartList[eventStartListIndex].startDrawSlot == -1 ) {
          // there were no empty slots, see if some existing event could be thinner
          for( var m = currEventSlots.length - 1; m > 0; m-- )
            if( currEventSlots[m] == currEventSlots[m-1] ) {
              dayEventStartList[eventStartListIndex].drawSlotCount = 1;
              currEventSlots[m] = dayEventStartList[eventStartListIndex];
              currEventSlots[m-1].drawSlotCount--;
              break;
            }
            
          if( dayEventStartList[eventStartListIndex].startDrawSlot == -1 ) {
            
            //event's not yet placed: must add a new slot
            currEventSlots[currEventSlots.length] = dayEventStartList[eventStartListIndex];
            dayEventStartList[eventStartListIndex].startDrawSlot = i;
            dayEventStartList[eventStartListIndex].drawSlotCount = 1;
          }
        }
        eventStartListIndex++;
      } else {
        
        // nextEventIsStarting==false, remove one event from the slots
        currEventSlotsIsEmpty = true;
        for( i = 0; i < currEventSlots.length; i++ ) {
          if( currEventSlots[i] == dayEventEndList[eventEndListIndex] )
            currEventSlots[i] = null;
          
          if( currEventSlots[i] != null )
            currEventSlotsIsEmpty = false;
        }
        
        if( currEventSlotsIsEmpty ) {
          // There are no events in the slots, so we can set totalSlotCount 
          // for the previous contiguous group of events
          for( i = groupStartIndex; i < eventStartListIndex; i++ ) {
            dayEventStartList[i].totalSlotCount = currEventSlots.length;
          }
          currEventSlots.length = 0;
          groupStartIndex = eventStartListIndex;
        }         
        eventEndListIndex++;
      }
       
      //update vars for next loop
      if( dayEventStartList[eventStartListIndex] == null )
        done = true;
      else
        nextEventIsStarting = ( dayEventStartList[eventStartListIndex].displayDate <  dayEventEndList[eventEndListIndex].displayEndDate );     
    }
    //  set totalSlotCount for the last contiguous group of events
    for ( i = groupStartIndex; i < dayEventStartList.length; i++ )
      dayEventStartList[i].totalSlotCount = currEventSlots.length;
  }
}

/** PRIVATE
 *
 *  Sets the following all-day event draw order properties. 
 *  Used in day view and week view.
 *    allDayStartsBefore 
 *      (true if event starts before startDate)
 *    allDayEndsAfter 
 *      (true if event ends after endDate)
 *    startDrawSlot
 *      (index of the day the event will be drawn on. Used in weekview)
 *    drawSlotCount
 *      (count of days the event occupies between startDate and endDay. Used in weekview)
 *    drawRow
 *      (which allday-row the event should be drawn on. Used in weekview)
 */
CalendarView.prototype.setAllDayDrawProperties = function calView_setAllDayDrawProperties( eventList, startDate, endDate) {
  startDate.setHours( 0, 0, 0, 0 );
  
  if ( !endDate ) {
    endDate = new Date(startDate);
    endDate.setDate( endDate.getDate() + 1 );
  }
  endDate.setHours( 0, 0, 0, 0 );
  
  var startDateValue = startDate.valueOf();
  var endDateValue = endDate.valueOf();
  
  var recurObj = new Object();
  var starttime;
  var endtime;
  
  var dayCount = ( endDateValue - startDateValue ) / kDate_MillisecondsInDay;
  var row;
  var rowFound;
  
  var usedSlotMatrix = new Array(); //all-day rows 
  usedSlotMatrix.push( new Array() ); //first row, one column per day between startDate and endDate
  var i;
  for( i = 0; i < dayCount; i++ ) {
    usedSlotMatrix[0].push( false );
  }
  
  for( i = 0; i < eventList.length; i++ ) {
    if( eventList[i].event.allDay) {
      
      if( eventList[i].event.recur ) {
        //get start time for correct occurrence
        
        //HACK: didn't know how to get the correct occurrence, ended 
        // up just selecting the last one... This is definitely wrong,
        // and should be fixed when recurring event occurrences are 
        // easier to handle (bug 242544)
        if( eventList[i].event.getPreviousOccurrence( endDate.getTime(), recurObj ) ) {
          starttime = recurObj.value;
          endtime = starttime + (eventList[i].event.end.getTime() -
                                 eventList[i].event.start.getTime());
        }
      } else {
        starttime = eventList[i].event.start.getTime();
        endtime = eventList[i].event.end.getTime();
      }
      
      if( starttime < startDateValue ) {
        eventList[i].allDayStartsBefore = true;
        eventList[i].startDrawSlot=0;
      } else {
        eventList[i].allDayStartsBefore = false;
        eventList[i].startDrawSlot = (starttime - startDateValue) / kDate_MillisecondsInDay;
      }
      if( endtime > endDateValue ) {
        eventList[i].allDayEndsAfter = true;
        eventList[i].drawSlotCount = ((endDateValue - startDateValue) / kDate_MillisecondsInDay)-eventList[i].startDrawSlot;
      } else {
        eventList[i].allDayEndsAfter = false;
        eventList[i].drawSlotCount = ((endtime - startDateValue) / kDate_MillisecondsInDay)-eventList[i].startDrawSlot;
      }
      
      rowFound = false;
      row=0;
      var k;
      while( ( !rowFound ) && ( row < usedSlotMatrix.length ) ) {
        //check if there is room for the event on the row
        rowFound = true;
        for( k = eventList[i].startDrawSlot; k < (eventList[i].startDrawSlot + eventList[i].drawSlotCount); k++) {
          if( usedSlotMatrix[row][k] ) {
            rowFound = false;
            break;
          }
        }
        if( !rowFound ) {
          row++;
        }
      }
      if(!rowFound) {
        //add new all-day-row
        var newArray = new Array(); 
        for( k = 0; k < dayCount; k++ ) {
          newArray.push( false );
        }
        usedSlotMatrix.push(newArray);
        row = usedSlotMatrix.length - 1;
      }
      
      eventList[i].drawRow = row;
      for( k = eventList[i].startDrawSlot; k < (eventList[i].startDrawSlot + eventList[i].drawSlotCount); k++) {
        usedSlotMatrix[row][k] = true;
      }
    }
  }
}

/** PROTECTED 

    Common function to remove elements with attribute from DOM tree.
    
    Works around diffences in getElementsByAttribute list between
    Moz1.8+, and Moz1.7- (Moz1.8+ list contents is 'live', and changes
    as elements are removed from dom, while in Moz1.7- list is not 'live').
 **/
CalendarView.prototype.removeElementsByAttribute =
  function calView_removeElementsByAttribute(attributeName, attributeValue) 
{
  var liveList = document.getElementsByAttribute(attributeName, attributeValue);
  // Delete in reverse order.  Moz1.8+ getElementsByAttribute list is
  // 'live', so when an element is deleted the indexes of later elements
  // change, but in Moz1.7- list is 'dead'.  Reversed order works with both.
  for (var i = liveList.length - 1; i >= 0; i--) {
    var element = liveList.item(i);
    if (element.parentNode != null) 
      element.parentNode.removeChild(element);
  }
}

/** PROTECTED 

    Common function to clear a marker attribute from elements in DOM tree.

    Works around diffences in getElementsByAttribute list between
    Moz1.8+, and Moz1.7- (Moz1.8+ list contents is 'live', and changes
    as attributes are removed from dom, while in Moz1.7- it is not 'live').
 **/
CalendarView.prototype.removeAttributeFromElements =
  function calView_removeAttributeFromElements(attributeName, attributeValue) 
{
  var liveList = document.getElementsByAttribute(attributeName, attributeValue);
  // Delete in reverse order.  Moz1.8+ getElementsByAttribute list is
  // 'live', so when an attribute is deleted the indexes of later elements
  // change, but in Moz1.7- list is 'dead'.  Reversed order works with both.
  for (var i = liveList.length - 1; i >= 0; i--) {
    liveList.item(i).removeAttribute(attributeName);
  }
}
