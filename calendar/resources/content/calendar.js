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
 *                 Mike Norton <xor@ivwnet.com>
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

//the next line needs XX-DATE-XY but last X instead of Y
var gDateMade = "2002052213-cal"

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

var gClockId;

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
var kDaysInWeek = 7;

const kMAX_NUMBER_OF_DOTS_IN_MONTH_VIEW = "8"; //the maximum number of dots that fit in the month view


var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService);
var rootPrefNode = prefService.getBranch(null); // preferences root node

/*-----------------------------------------------------------------
*  G L O B A L     C A L E N D A R      F U N C T I O N S
*/


/** 
* Called from calendar.xul window onload.
*/

function calendarInit() 
{
	// get the calendar event data source
   gEventSource = new CalendarEventDataSource(  );
   
   // get the Ical Library
   gICalLib = gEventSource.getICalLib();
   
   // set up the CalendarWindow instance
   
   gCalendarWindow = new CalendarWindow( gEventSource );
   
   gCalendarWindow.currentView.switchTo( gCalendarWindow.currentView );

   gCalendarWindow.currentView.refresh( gCalendarWindow.currentView );

   // set up the unifinder
   
   prepareCalendarUnifinder( );

   prepareCalendarToDoUnifinder( );
   
   update_date( );
   	
	checkForMailNews();
}

// Set the date and time on the clock and set up a timeout to refresh the clock when the 
// next minute ticks over

function update_date( )
{
   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   gCalendarWindow.currentView.hiliteTodaysDate();

   gClockId = setTimeout( "update_date( )", milliSecsTillTomorrow ); 
}

/** 
* Called from calendar.xul window onunload.
*/

function calendarFinish()
{
   finishCalendarUnifinder( );
   
   finishCalendarToDoUnifinder( );

   gCalendarWindow.close( );
}

function launchPreferences()
{
   goPreferences("advancedItem", "chrome://calendar/content/pref/calendarPref.xul", "calendar");
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
   gCalendarWindow.EventSelection.replaceSelection( eventBox.calendarEventDisplay.event );
   
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

function dayViewHourClick( event )
{
   gCalendarWindow.setSelectedHour( event.target.getAttribute( "hour" ) );
}


/**
* Called on double click of an hour box.
*/

function dayViewHourDoubleClick( event )
{
   // change the date selection to the clicked hour
   
   gCalendarWindow.setSelectedHour( event.target.getAttribute( "hour" ) );
   
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
   gCalendarWindow.EventSelection.replaceSelection( eventBox.calendarEventDisplay.event );

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

function weekViewHourClick( event )
{
   var dayIndex = event.target.getAttribute( "day" );

   newDate = gHeaderDateItemArray[dayIndex].getAttribute( "date" );

   gCalendarWindow.setSelectedDate( newDate );

   gCalendarWindow.setSelectedHour( event.target.getAttribute( "hour" ) );
}


/**
* Called on double click of an hour box.
*/

function weekViewHourDoubleClick( event )
{
   var dayIndex = event.target.getAttribute( "day" );

   newDate = gHeaderDateItemArray[dayIndex].getAttribute( "date" );

   gCalendarWindow.setSelectedDate( newDate );

   // change the date selection to the clicked hour
   
   gCalendarWindow.setSelectedHour( event.target.getAttribute( "hour" ) );
   
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
   gCalendarWindow.MonthView.clickEventBox( eventBox, event );

   if ( event ) 
   {
      event.stopPropagation();
   }
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
   gCalendarWindow.monthView.clearSelectedDate( );
   
   editEvent( eventBox.calendarEventDisplay.event );

   if ( event ) 
   {
      event.stopPropagation();
   }
   
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


/** 
* Called when the new event button is clicked
*/

function newToDoCommand()
{
   var calendarToDo = createToDo();

   var dueDate = gCalendarWindow.currentView.getNewEventDate();
   
   
   var Minutes = Math.ceil( dueDate.getMinutes() / 5 ) * 5 ;
   
   dueDate = new Date( dueDate.getFullYear(),
                       dueDate.getMonth(),
                       dueDate.getDate(),
                       dueDate.getHours(),
                       Minutes,
                       0);
   

   calendarToDo.due.setTime( dueDate );
   
   var args = new Object();
   args.mode = "new";
   args.onOk =  self.addToDoDialogResponse;
   args.calendarToDo = calendarToDo;

   // open the dialog modally
   openDialog("chrome://calendar/content/calendarToDoDialog.xul", "caEditEvent", "chrome,modal", args );
}


function createEvent ()
{
   var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
   var iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
   return iCalEvent;
}


function createToDo ()
{
   var iCalToDoComponent = Components.classes["@mozilla.org/icaltodo;1"].createInstance();
   var iCalToDo = iCalToDoComponent.QueryInterface(Components.interfaces.oeIICalTodo);
   return iCalToDo;
}



/** 
* Helper function to launch the event composer to create a new event.
* When the user clicks OK "addEventDialogResponse" is called
*/

function newEvent( startDate, endDate )
{
   // create a new event to be edited and added
   var calendarEvent = createEvent();
   
   calendarEvent.start.setTime( startDate );
   
   if( !endDate )
   {
      var MinutesToAddOn = gCalendarWindow.calendarPreferences.getPref( "defaulteventlength" );
   
      var endDateTime = startDate.getTime() + ( 1000 * 60 * MinutesToAddOn );
   
      calendarEvent.end.setTime( endDateTime );
   }
   else
   {
      calendarEvent.end.setTime( endDate.getTime() );
   }
   editNewEvent( calendarEvent );
}


/**
* Helper function to launch the event composer to edit a new event.
* When the user clicks OK "addEventDialogResponse" is called
*/

function editNewEvent( calendarEvent )
{
   // set up a bunch of args to pass to the dialog

   var args = new Object();
   args.mode = "new";
   args.onOk =  self.addEventDialogResponse;
   args.calendarEvent = calendarEvent;

   // open the dialog modally
   openDialog("chrome://calendar/content/calendarEventDialog.xul", "caEditEvent", "chrome,modal", args );
}


/** 
* Called when the user clicks OK in the new event dialog
*
* Update the data source, the unifinder views and the calendar views will be
* notified of the change through their respective observers
*/

function addEventDialogResponse( calendarEvent, ArrayOfExceptionDates )
{
   gICalLib.addEvent( calendarEvent );

   for( i = 0; i < ArrayOfExceptionDates.length; i++ )
   {
      calendarEvent.addException( ArrayOfExceptionDates[i] );
   }
   gICalLib.modifyEvent( calendarEvent );
}


/** 
* Called when the user clicks OK in the new to do item dialog
*
*/

function addToDoDialogResponse( calendarToDo )
{
   gICalLib.addTodo( calendarToDo );
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
* Helper function to launch the event composer to edit an event.
* When the user clicks OK "modifyEventDialogResponse" is called
*/

function editToDo( calendarToDo )
{
   // set up a bunch of args to pass to the dialog
   
   var args = new Object();
   args.mode = "edit";
   args.onOk = self.modifyToDoDialogResponse;           
   args.calendarToDo = calendarToDo;
   
   // open the dialog modally
   
   openDialog("chrome://calendar/content/calendarToDoDialog.xul", "caEditToDo", "chrome,modal", args );
}
   

/** 
* Called when the user clicks OK in the edit event dialog
*
* Update the data source, the unifinder views and the calendar views will be
* notified of the change through their respective observers
*/

function modifyEventDialogResponse( calendarEvent, ArrayOfExceptionDates )
{
   calendarEvent.removeAllExceptions();

   //remove all the old exceptions
   
   for( i = 0; i < ArrayOfExceptionDates.length; i++ )
   {
      calendarEvent.addException( ArrayOfExceptionDates[i] );
   }

   gICalLib.modifyEvent( calendarEvent );
}


/** 
* Called when the user clicks OK in the edit event dialog
*
* Update the data source, the unifinder views and the calendar views will be
* notified of the change through their respective observers
*/

function modifyToDoDialogResponse( calendarToDo )
{
   gICalLib.modifyTodo( calendarToDo );
}


/**
*  Called when a user hovers over an element and the text for the mouse over is changed.
*/

function getPreviewText( calendarEventDisplay )
{
	var HolderBox = document.createElement( "vbox" );

   if (calendarEventDisplay.event.title)
   {
      var TitleHtml = document.createElement( "description" );
      var TitleText = document.createTextNode( "Title: "+calendarEventDisplay.event.title );
      TitleHtml.appendChild( TitleText );
      HolderBox.appendChild( TitleHtml );
   }

   var DateHtml = document.createElement( "description" );
   var DateText = document.createTextNode( "Start: "+calendarEventDisplay.event.start.toString() );
   DateHtml.appendChild( DateText );
   HolderBox.appendChild( DateHtml );

   if (calendarEventDisplay.event.description)
   {
      var DescriptionHtml = document.createElement( "description" );
      var DescriptionText = document.createTextNode( "Description: "+calendarEventDisplay.event.description );
      DescriptionHtml.appendChild( DescriptionText );
      HolderBox.appendChild( DescriptionHtml );
   }

   return ( HolderBox );
}

function alertCalendarVersion()
{
   window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", 'chrome://calendar/content/about.html' );
}

function playSound( ThisURL )
{
   ThisURL = "chrome://calendar/content/sound.wav";

   var url = Components.classes["@mozilla.org/network/standard-url;1"].createInstance();
   url = url.QueryInterface(Components.interfaces.nsIURL);
   url.spec = ThisURL;

   var sample = Components.classes["@mozilla.org/sound;1"].createInstance();
   
   sample = sample.QueryInterface(Components.interfaces.nsISound);

   sample.play( url );
}

function selectAllEvents()
{
   gCalendarWindow.EventSelection.setArrayToSelection( gCalendarWindow.eventSource.getCurrentEvents() );
}
