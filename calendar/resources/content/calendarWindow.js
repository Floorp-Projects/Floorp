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

function CalendarWindow( calendarDataSource )
{
   this.eventSource = calendarDataSource;
   
   //setup the preferences
   this.calendarPreferences = new calendarPreferences( this );
   
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

               //if( calendarWindow.currentView == calendarWindow.monthView )
               //{
               //   calendarWindow.currentView.hiliteSelectedDate( );
               //}
            }
         }
      },
      onAlarm : function( calendarEvent )
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
*   Display today in the current view
*/

CalendarWindow.prototype.goToToday = function calWin_goToToday( )
{
   this.clearSelectedEvent( );

   this.currentView.goToDay( new Date() )
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

CalendarWindow.prototype.setSelectedDate = function calWin_setSelectedDate( date )
{
   // Copy the date because we might mess with it in place
   
   this.selectedDate = new Date( date );

   /* on some machines, we need to check for .selectedItem first */
   if( document.getElementById( "event-filter-menulist" ).selectedItem &&
       document.getElementById( "event-filter-menulist" ).selectedItem.value == "current" )
   {
      //redraw the top tree
      setTimeout( "refreshEventTree( getAndSetEventTable() );", 150 );
   }
   document.getElementById( "lefthandcalendar" ).value = date;
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
   
   var HolderBox = getPreviewTextForRepeatingEvent( event.currentTarget.calendarEventDisplay );
   
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
*   Use this function to compare two numbers. Returns the difference.
*   This is used to sort an array, and sorts it in ascending order.
*
*/

CalendarWindow.prototype.compareNumbers = function calWin_compareNumbers(a, b) {
   return a - b
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
   
   if(this.calendarWindow.currentView.doResize)
      this.calendarWindow.currentView.doResize();
   
   this.refreshEvents()
}

