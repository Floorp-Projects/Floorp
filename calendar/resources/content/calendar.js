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

/***** calendar
* AUTHOR
*   Garth Smedley
*
* REQUIRED INCLUDES 
*        <script type="application/x-javascript" src="chrome://global/content/strres.js"/>
*        <script type="application/x-javascript" src="chrome://calendar/content/calendarEvent.js"/>
*
* NOTES
*   Code for the oe-calendar.
*
*   What is in this file:
*     - Global variables and functions - Called directly from the XUL
*     - Several classes:
*           CalendarWindow - Owns the 3 subviews, keeps track of the selection etc.
*           CalendarView - Super class for three views
*                  MonthView 
*                  WeekView 
*                  DayView 
*     - Unifinder functions - fill in content of the unifinder.
*  
* IMPLEMENTATION NOTES 
*
*  WARNING  - We currently use CalendarEvent objects in what will be an unsafe way
*             when the datasource changes. We assume that there is exactly one instance of CalendarEvent for 
*             each unique event. We will probably have to switch to using the calendar event id 
*             property instead of the object. G.S. April 2001.
**********
*/






/*-----------------------------------------------------------------
*  G L O B A L     V A R I A B L E S
*/

// turn on debuging

var gDebugCalendar = false;

// ICal Library

var gICalLib = null;

// calendar event data source see penCalendarEvent.js

var gEventSource = null;

// single global instance of CalendarWindow

var gCalendarWindow;

//an array of indexes to boxes for the week view

var gHeaderDateItemArray = null;

// Show event details on mouseover

var showEventDetails = true;


// DAY VIEW VARIABLES
var kDayViewHourHeight = 50;
var kDayViewHourWidth = 105;
var kDayViewHourLeftStart = 105;

var kWeekViewHourHeight = 50;
var kWeekViewHourWidth = 78;
var kWeekViewItemWidth = 20;
var kWeekViewHourLeftStart = 97;
var kWeekViewHourHeightDifference = 2;

const kMAX_NUMBER_OF_DOTS_IN_MONTH_VIEW = "8"; //the maximum number of dots that fit in the month view

/*-----------------------------------------------------------------
*  G L O B A L     C A L E N D A R      F U N C T I O N S
*/


/** 
* Called from calendar.xul window onload.
*/

function calendarInit() 
{
	// get the calendar event data source
   // global calendar
   gEventSource = new CalendarEventDataSource(  );
   
   // get the Ical Library
   gICalLib = gEventSource.getICalLib();
   
   // set up the CalendarWindow instance
   
   gCalendarWindow = new CalendarWindow( gEventSource );
   
   // set up the unifinder
   
   prepareCalendarUnifinder( gEventSource );

   // show the month view, with today selected
   
   gCalendarWindow.switchToMonthView( );

   update_date( );

   deselectEventInUnifinder();
   //sizeToContent();
   //window.resizeTo(1024,708);
}

// Set the date and time on the clock and set up a timeout to refresh the clock when the 
// next minute ticks over

function update_date( Refresh )
{
   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   gCalendarWindow.currentView.hiliteTodaysDate();

   var gClockId = setTimeout( "update_date( )", milliSecsTillTomorrow ); 
}

/** 
* Called from calendar.xul window onunload.
*/

function calendarFinish()
{
   finishCalendarUnifinder( gEventSource );
   
   gCalendarWindow.close( );
}


/** 
* Called to set up the date picker from the go to day button
*/

function prepareChooseDate()
{
   // the value attribute of the datePickerPopup is the initial date shown
   
   var datePickerPopup = document.getElementById( "oe-date-picker-popup" );   
   
   datePickerPopup.setAttribute( "value", gCalendarWindow.getSelectedDate() );
}


/** 
* Called on single click in the day view, select an event
*
* PARAMETERS
*    hourNumber - 0-23 hard-coded in the XUL
*    event      - the click event, Not used yet 
*/

function dayEventItemClick( eventBox, event )
{
   gCalendarWindow.dayView.clickEventBox( eventBox, event );
   
   if ( event ) 
   {
      event.stopPropagation();
   }
}


/** 
* Called on double click in the day view, edit an existing event
* or create a new one.
*
* PARAMETERS
*    hourNumber - 0-23 hard-coded in the XUL
*    event      - the click event, Not used yet 
*/

function dayEventItemDoubleClick( eventBox, event )
{
   editEvent( eventBox.calendarEventDisplay.event );

   if ( event ) 
   {
      event.stopPropagation();
   }
}


/** 
* Called on single click in the hour area in the day view
*
* PARAMETERS
*    hourNumber - 0-23 hard-coded in the XUL
*    event      - the click event, Not used yet 
*/

function dayViewHourClick( hourNumber, event )
{
   gCalendarWindow.setSelectedHour( hourNumber );
}


/**
* Called on double click of an hour box.
*/

function dayViewHourDoubleClick( hourNumber, event )
{
   // change the date selection to the clicked hour
   
   gCalendarWindow.setSelectedHour( hourNumber );
   
   var startDate = gCalendarWindow.dayView.getNewEventDate();
   newEvent( startDate );
}


/** 
* Called on single click in the day view, select an event
*
* PARAMETERS
*    hourNumber - 0-23 hard-coded in the XUL
*    event      - the click event, Not used yet 
*/

function weekEventItemClick( eventBox, event )
{
   gCalendarWindow.weekView.clickEventBox( eventBox, event );

   if ( event ) 
   {
      event.stopPropagation();
   }
   
}


/** 
* Called on double click in the day view, edit an existing event
* or create a new one.
*
* PARAMETERS
*    hourNumber - 0-23 hard-coded in the XUL
*    event      - the click event, Not used yet 
*/

function weekEventItemDoubleClick( eventBox, event )
{
   editEvent( eventBox.calendarEventDisplay.event );

   if ( event ) 
   {
      event.stopPropagation();
   }
}

/** ( event )
* Called on single click in the hour area in the day view
*
* PARAMETERS
*    hourNumber - 0-23 hard-coded in the XUL
*    event      - the click event, Not used yet 
*/

function weekViewHourClick( dayIndex, hourNumber, event )
{
   newDate = gHeaderDateItemArray[dayIndex].getAttribute( "date" );

   gCalendarWindow.setSelectedDate( newDate );

   gCalendarWindow.setSelectedHour( hourNumber );
}


/**
* Called on double click of an hour box.
*/

function weekViewHourDoubleClick( dayIndex, hourNumber, event )
{
   newDate = gHeaderDateItemArray[dayIndex].getAttribute( "date" );

   gCalendarWindow.setSelectedDate( newDate );

   // change the date selection to the clicked hour
   
   gCalendarWindow.setSelectedHour( hourNumber );
   
   var startDate = gCalendarWindow.weekView.getNewEventDate();
   newEvent( startDate );
   
}


/** 
* Called on single click on an event box in the month view
*
* PARAMETERS
*    eventBox - The XUL box clicked on
*    event      - the click event
*/

function monthEventBoxClickEvent( eventBox, event )
{
   gCalendarWindow.monthView.clickEventBox( eventBox, event );
}


/** 
* Called on double click on an event box in the month view, 
* launches the edit dialog on the event
*
* PARAMETERS
*    eventBox - The XUL box clicked on
*/

function monthEventBoxDoubleClickEvent( eventBox, event )
{
   selectEventInUnifinder( eventBox.calendarEventDisplay.event );

   gCalendarWindow.monthView.clearSelectedDate( );
   
   editEvent( eventBox.calendarEventDisplay.event );

   if ( event ) 
   {
      event.stopPropagation();
   }
   
}
   

/** 
*   Called when an event is clicked on.
*   Hightlights the event in the unifinder
*/

function selectEventInUnifinder( calendarEvent )
{
   gUnifinderSelection = calendarEvent.id;

   var Tree = document.getElementById( "unifinder-categories-tree" );
   
   var TreeItem = document.getElementById( "unifinder-treeitem-"+gUnifinderSelection );
   
   if ( Tree && TreeItem )
   {
      Tree.selectItem( TreeItem );

   }

   document.getElementById( "unifinder-remove-button" ).removeAttribute( "disabled" );

   document.getElementById( "unifinder-modify-button" ).removeAttribute( "disabled" );

   document.getElementById( "unifinder-modify-menu" ).removeAttribute( "disabled" );
   
   document.getElementById( "unifinder-remove-menu" ).removeAttribute( "disabled" );
}


/** 
*   Called when a day is clicked on.
*   deSelects the selected item in the unifinder.
*/

function deselectEventInUnifinder( )
{
   if ( gUnifinderSelection ) 
   {
      var Tree = document.getElementById( "unifinder-categories-tree" );
      
      //HACK THIS DOESN"T WORK RIGHT NOW FOR MIKE POTTER????
      //Tree.clearItemSelection( );
      
      gUnifinderSelection = null;
   }

   document.getElementById( "unifinder-remove-button" ).setAttribute( "disabled", true );

   document.getElementById( "unifinder-modify-button" ).setAttribute( "disabled", true );

   document.getElementById( "unifinder-modify-menu" ).setAttribute( "disabled", true );
   
   document.getElementById( "unifinder-remove-menu" ).setAttribute( "disabled", true );
}
   
function selectCategoryInUnifinder( )
{
   document.getElementById( "unifinder-remove-button" ).removeAttribute( "disabled" );

   document.getElementById( "unifinder-modify-button" ).removeAttribute( "disabled" );

   document.getElementById( "unifinder-modify-menu" ).removeAttribute( "disabled" );
   
   document.getElementById( "unifinder-remove-menu" ).removeAttribute( "disabled" );
}

function deselectCategoryInUnifinder( )
{
   document.getElementById( "unifinder-remove-button" ).setAttribute( "disabled", true );

   document.getElementById( "unifinder-modify-button" ).setAttribute( "disabled", true );

   document.getElementById( "unifinder-modify-menu" ).setAttribute( "disabled", true );
   
   document.getElementById( "unifinder-remove-menu" ).setAttribute( "disabled", true );
}


/** 
* Called when the new event button is clicked
*/

function newEventCommand()
{
   var startDate = gCalendarWindow.currentView.getNewEventDate();
   
   
   var Minutes = Math.ceil( startDate.getMinutes() / 5 ) * 5 ;
   
   startDate = new Date( startDate.getFullYear(),
                         startDate.getMonth(),
                         startDate.getDate(),
                         startDate.getHours(),
                         Minutes,
                         0);
   newEvent( startDate );
}


function createEvent ()
{
   var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
   var iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
   return iCalEvent;
}





/** 
* Helper function to launch the event composer to create a new event.
* When the user clicks OK "addEventDialogResponse" is called
*/

function newEvent( startDate )
{
   // set up a bunch of args to pass to the dialog
   
   var args = new Object();
   
   args.mode = "new";
   
   args.onOk =  self.addEventDialogResponse; 
   
   args.calendarEvent = createEvent();
   
   args.calendarEvent.start.setTime( startDate );
   args.calendarEvent.end.setTime( startDate );
   args.calendarEvent.end.hour++;

   // open the dialog non modally
            
   openDialog( "chrome://calendar/content/calendarEventDialog.xul", "caNewEvent", "chrome,modal", args );
}


/** 
* Called when the user clicks OK in the new event dialog
*
* Update the data source, the unifinder views and the calendar views will be
* notified of the change through their respective observers
*/

function addEventDialogResponse( calendarEvent )
{
   gICalLib.addEvent( calendarEvent );
}


/** 
* Helper function to launch the event composer to edit an event.
* When the user clicks OK "modifyEventDialogResponse" is called
*/

function editEvent( calendarEvent )
{
   // set up a bunch of args to pass to the dialog
   
   var args = new Object();
   args.mode = "edit";
   args.onOk = self.modifyEventDialogResponse;           
   args.calendarEvent = calendarEvent;

   // open the dialog modally
   
   openDialog("chrome://calendar/content/calendarEventDialog.xul", "caEditEvent", "chrome,modal", args );
}
   

/** 
* Called when the user clicks OK in the edit event dialog
*
* Update the data source, the unifinder views and the calendar views will be
* notified of the change through their respective observers
*/

function modifyEventDialogResponse( calendarEvent )
{
   gICalLib.modifyEvent( calendarEvent );
   
}


/**
*  Called when a user hovers over an element and the text for the mouse over is changed.
*/

function getPreviewText( calendarEventDisplay )
{
   var TextToReturn = " At "+calendarEventDisplay.displayDate+" you have an event titled: "+calendarEventDisplay.event.title;
	var HolderBox = document.createElement( "vbox" );

   var TitleHtml = document.createElement( "description" );
   var TitleText = document.createTextNode( "Title: "+calendarEventDisplay.event.title );
   TitleHtml.appendChild( TitleText );
   HolderBox.appendChild( TitleHtml );

   var DateHtml = document.createElement( "description" );
   var DateText = document.createTextNode( "Start: "+calendarEventDisplay.event.start.toString() );
   DateHtml.appendChild( DateText );
   HolderBox.appendChild( DateHtml );

   /*TimeHtml = document.createElement( "description" );
   TimeText = document.createTextNode( calendarEvent.start.toString() );
   TimeHtml.appendChild( TimeText );
   HolderBox.appendChild( TimeHtml );
   */
   var DescriptionHtml = document.createElement( "description" );
   var DescriptionText = document.createTextNode( "Description: "+calendarEventDisplay.event.description );
   DescriptionHtml.appendChild( DescriptionText );
   HolderBox.appendChild( DescriptionHtml );


   return ( HolderBox );
}



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
   
   this.dateFormater = new DateFormater();
   
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
                calendarWindow.setSelectedEvent( calendarEvent );
                calendarWindow.currentView.clearSelectedDate( );
                calendarWindow.currentView.refreshEvents( );
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
             calendarWindow.currentView.clearSelectedDate( );
                
             calendarWindow.currentView.refreshEvents( );
        }
      },
   
      onDeleteItem : function( calendarEvent, nextEvent )
      {
        calendarWindow.clearSelectedEvent( calendarEvent );
        
        if( !gICalLib.batchMode )
        {
            if ( nextEvent ) 
            {
                calendarWindow.setSelectedEvent( nextEvent );
                
                calendarWindow.currentView.clearSelectedDate( );
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
            calendarWindow.currentView.refreshEvents( );
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

CalendarWindow.prototype.goToNext = function()
{
   this.clearSelectedEvent( );

   this.currentView.goToNext();
}


/** PUBLIC
*
*   Go to the previous period in the current view
*/

CalendarWindow.prototype.goToPrevious = function()
{   
   this.clearSelectedEvent( );

   this.currentView.goToPrevious( );
}


/** PUBLIC
*
*   Go to today in the current view
*/

CalendarWindow.prototype.goToDay = function( newDate )
{
   this.clearSelectedEvent( );

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
   this.selectedEvent = selectedEvent;

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
   
   if( unSelectedEvent === undefined ||
       unSelectedEvent == null || 
       unSelectedEvent === this.selectedEvent )
   {
      this.currentView.clearSelectedEvent( );
      this.selectedEvent = null;

   }
}


/** PUBLIC
*
*   Returns the selected event, or NULL if none selected
*/

CalendarWindow.prototype.getSelectedEvent = function( )
{
   return this.selectedEvent;
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
   this.clearSelectedEvent ( this.selectedEvent );
   
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
}

   
function reloadApplication()
{
	gCalendarWindow.currentView.refreshEvents( );
}
