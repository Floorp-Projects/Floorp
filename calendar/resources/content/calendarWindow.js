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
   
   this.EventSelection = new CalendarEventSelection( this );

   this.dateFormater = new DateFormater( this );
   
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
   this.switchToMonthView();
   
   // now that all is set up we can start to observe the data source
   
   // make the observer, the calendarEventDataSource calls into the
   // observer when things change in the data source.
   
   var calendarWindow = this;
   
   this.calendarEventDataSourceObserver =
   {
      
      onLoad   : function()
      {
         // called when the data source has finished loading
         
         calendarWindow.currentView.refreshEvents( );
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
                calendarWindow.currentView.refreshEvents( );
                
                calendarWindow.setSelectedEvent( calendarEvent );
             }
         }
      },
   
      onModifyItem : function( calendarEvent, originalEvent )
      {
        if( !gICalLib.batchMode )
        {
             if( calendarEvent )
             {
                calendarWindow.setSelectedEvent( calendarEvent );
                
             }
             calendarWindow.currentView.refreshEvents( );
        }
      },
   
      onDeleteItem : function( calendarEvent, nextEvent )
      {
        calendarWindow.clearSelectedEvent( calendarEvent );
        
        if( !gICalLib.batchMode )
        {
            calendarWindow.currentView.refreshEvents( );
            
            if ( nextEvent ) 
            {
                calendarWindow.setSelectedEvent( nextEvent );
            }
            else
            {
                var eventStartDate = new Date( calendarEvent.start.getTime() );
                calendarWindow.setSelectedDate( eventStartDate );
                
                if( calendarWindow.currentView == calendarWindow.monthView )
                {
                   calendarWindow.currentView.hiliteSelectedDate( );
                }
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

CalendarWindow.prototype.close = function( )
{
//   this.eventSource.removeObserver(  this.calendarEventDataSourceObserver ); 
}


/** PUBLIC
*
*   Switch to the day view if it isn't already the current view
*/

CalendarWindow.prototype.switchToDayView = function( )
{
   this.switchToView( this.dayView )
}


/** PUBLIC
*
*   Switch to the week view if it isn't already the current view
*/

CalendarWindow.prototype.switchToWeekView = function( )
{
   this.switchToView( this.weekView )
}


/** PUBLIC
*
*   Switch to the month view if it isn't already the current view
*/

CalendarWindow.prototype.switchToMonthView = function( )
{
   this.switchToView( this.monthView )
}

/** PUBLIC
*
*   Display today in the current view
*/

CalendarWindow.prototype.goToToday = function( )
{
   this.clearSelectedEvent( );

   this.currentView.goToDay( new Date() )
}


/** PUBLIC
*
*   Go to the next period in the current view
*/

CalendarWindow.prototype.goToNext = function( value )
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

CalendarWindow.prototype.goToPrevious = function( value )
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

CalendarWindow.prototype.goToDay = function( newDate )
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

CalendarWindow.prototype.setSelectedEvent = function( selectedEvent )
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

CalendarWindow.prototype.clearSelectedEvent = function( unSelectedEvent )
{
   var undefined;
   
   gCalendarWindow.EventSelection.emptySelection( );
      
   if( unSelectedEvent === undefined ||
       unSelectedEvent == null )
   {
      this.currentView.clearSelectedEvent( );
   }
}


/** PUBLIC
*
*   Set the selected date
*/

CalendarWindow.prototype.setSelectedDate = function( date )
{
   // Copy the date because we might mess with it in place
   
   this.selectedDate = new Date( date );
}

/** PUBLIC
*
*   Get the selected date
*/

CalendarWindow.prototype.getSelectedDate = function( )
{
   // Copy the date because we might mess with it in place
   
   return new Date( this.selectedDate );
}


/** PUBLIC
*
*   Change the hour of the selected date
*/

CalendarWindow.prototype.setSelectedHour = function( hour )
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

CalendarWindow.prototype.switchToView = function( newView )
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

CalendarWindow.prototype.mouseOverInfo = function( calendarEvent, event )
{
   var Html = document.getElementById( "savetip" );

   while( Html.hasChildNodes() )
   {
      Html.removeChild( Html.firstChild ); 
   }
   
   var HolderBox = getPreviewText( event.currentTarget.calendarEventDisplay );
   
   Html.appendChild( HolderBox );
}

/** PRIVATE
*
*   This returns the lowest element not in the array
*  eg. array(0, 0, 1, 3) would return 2
*   Used to figure out where to put the day events.
*
*/

CalendarWindow.prototype.getLowestElementNotInArray = function( InputArray )
{
   var Temp = 1;
   var AllZero = true; //are all the elements in the array 0?
   //CAUTION: Watch the scope here.  This function is called from inside a nested for loop.
   //You can't have the index variable named anything that is used in those for loops.

   for ( Mike = 0; Mike < InputArray.length; Mike++ ) 
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

CalendarWindow.prototype.compareNumbers = function (a, b) {
   return a - b
}

/** PUBLIC
*   The resize handler, used to set the size of the views so they fit the screen.
*/
 window.onresize = CalendarWindow.prototype.doResize = function(){
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

CalendarView.prototype.goToDay = function( newDate, ShowEvent )
{
   this.calendarWindow.setSelectedDate( newDate ); 
   
   this.refresh( ShowEvent )
}

/** PUBLIC
*
*   Refresh display of events and the selection in the view
*/

CalendarView.prototype.refresh = function( ShowEvent )
{
   this.refreshDisplay( ShowEvent )
   
   this.refreshEvents()

   if(this.calendarWindow.currentView.doResize)
      this.calendarWindow.currentView.doResize();
}

