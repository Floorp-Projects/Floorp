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
 *                 Chris Allen <christopher.allen@mint.steelcase.com>
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

/***** calendar/ca-calender-event.js
* AUTHOR
*   Garth Smedley
* REQUIRED INCLUDES 
*
* NOTES
*   CalendarEventDataSource class:
*      Saves and loads calendar events, provides methods
*      for adding, deleting, modifying and searching calendar events.
*
*   CalendarEvent class:
*      Contains info about calendar events.
*
*   Also provides an observer interface for clients that need to be 
*   notified when calendar event data changes.
*  
* IMPLEMENTATION NOTES 
*   Currently uses the Ical library to store events.
*   Access to ical is through the libxpical xpcom object.
*
**********
*/

/*-----------------------------------------------------------------
*  G L O B A L     V A R I A B L E S
*/

/*-----------------------------------------------------------------
*   CalendarEventDataSource Class
*
*  Maintains all of the calendar events.
*
* PROPERTIES
*  observerList   - array of observers, see constructor notes.
*/

/**
*   CalendarEventDataSource Constructor.
* 
* PARAMETERS
*      observer     - An object that implements methods that are called when the 
*                     data source is modified. The following methods should be implemented 
*                     by the observer object.
*
                     {
                        onLoad       : function() {},                // called when server is ready
                        onAddItem    : function( calendarEvent ) {}, 
                        onModifyItem : function( calendarEvent, originalEvent ) {},
                        onDeleteItem : function( calendarEvent ) {},
                        onAlarm      : function( calendarEvent ) {},
                        onError      : function() {},
                     };
                     
                     These methods are now called synchronously, if you add an event the onAddItem
                     method will be called during your call to add the event.

*
* NOTES
*   Is a singleton, the first invocation creates an instance, subsequent calls return the same instance.
*/

function CalendarEventDataSource( )
{
   try {
        var iCalLibComponent = Components.classes["@mozilla.org/ical-container;1"].getService();
   } catch ( e ) {
       alert( 
           "The calendar cannot run due to the following error:\n"+
           "The ICAL Component is not registered properly. Please follow the instructions given at:\n"+
          "http://bugzilla.mozilla.org/attachment.cgi?id=122860&action=view\n"+
          "If these instructions don't solve the problem, please add yourself to the cc list of\n"+
          "bug 134432 at http://bugzilla.mozilla.org/show_bug.cgi?id=134432.\n"+
          "and give more feedback on your platform, Mozilla version, calendar install type:\n"+
          "(build or xpi), any errors you see on the console that you think is related and any\n"+
          " other problems you come across when following the above insructions.\n\n"+e );
          window.close();
   }
         
   this.gICalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICalContainer);
     
   this.currentEvents = new Array();
   
   this.prepareAlarms( );
}



CalendarEventDataSource.InitService = function calEvent_InitService( root )
{
   return new CalendarEventDataSource( null, root.getUserPath() );
}

// turn on/off debuging
CalendarEventDataSource.gDebug = true;

// Singleton CalendarEventDataSource variable.

CalendarEventDataSource.debug = function( str )
{
    if( CalendarEventDataSource.gDebug )
    {
        dump( "\n CalendarEventDataSource DEBUG: "+ str + "\n");
    }
}


/** PUBLIC
*
* NOTES
*   Called at start up after all services have been inited. 
*/

CalendarEventDataSource.prototype.onServiceStartup = function calEvent_onServiceStartup( root )
{
}



/** PUBLIC
*
*   CalendarEventDataSource/search.
*
*      First hacked in implementation of search
*
* NOTES
*   This should be search ing by all content, not just the beginning
*   of the title field, A LOT remains to be done.. 
*/

CalendarEventDataSource.prototype.search = function calEvent_search( searchText, fieldName )
{
   searchText = searchText.toLowerCase();
   
   /*
   ** Try to get rid of all the spaces in the search text.
   ** At present, this only gets rid of one.. I don't know why.
   */
   var regexp = "\s+";
   searchText = searchText.replace( regexp, "" );
   
   var searchEventTable = new Array();
   
   if( searchText != "" )
   {
      var eventTable = this.currentEvents;
      
      for( var index = 0; index < eventTable.length; ++index )
      {
         var calendarEvent = eventTable[ index ];
         
         if ( typeof fieldName == "string") 
         {
            var value = calendarEvent[ fieldName ].toLowerCase();
         
            if( value )
            {
               if( value.indexOf( searchText ) != -1 )
               {
                  searchEventTable[ searchEventTable.length ]  =  calendarEvent; 
                  break;
               }
            }
         }
         else if ( typeof fieldName == "object" ) 
         {
            for ( i in fieldName ) 
            {
               var objValue = calendarEvent[ fieldName[i] ];
         
               if( objValue )
               {
                  objValue = objValue.toLowerCase();
                  if( objValue.indexOf( searchText ) != -1 )
                  {
                     searchEventTable[ searchEventTable.length ]  =  calendarEvent; 
                     break;
                  }
               }
            }
         }
      }
   }
   searchEventTable.sort( this.orderRawEventsByDate );

   return searchEventTable;
}

CalendarEventDataSource.prototype.searchBySql = function calEvent_searchBySql( Query )
{
   var eventDisplays = new Array();

   var eventList = this.gICalLib.searchBySQL( Query );

   while( eventList.hasMoreElements() )
   {
      eventDisplays[ eventDisplays.length ] = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
   }

   eventDisplays.sort( this.orderRawEventsByDate );
   
   return eventDisplays;
}

/** PUBLIC
*
*   CalendarEventDataSource/getEventsForDay.
*
* PARAMETERS
*      date     - Date object, uses the month and year and date  to get all events 
*                 for the given day.
* RETURN
*      array    - of events for the day
*/


CalendarEventDataSource.prototype.getEventsForDay = function calEvent_getEventsForDay( date )
{
   var eventDisplays =  new Array();

   var eventList = this.gICalLib.getEventsForDay( date );
   
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEventDisplay);
      
      var EventObject = new Object;
      
      EventObject.event = tmpevent.event;
      
      EventObject.displayDate = tmpevent.displayDate;
      EventObject.displayEndDate = tmpevent.displayEndDate;
      
      eventDisplays[ eventDisplays.length ] = EventObject;
   }

   eventDisplays.sort( this.orderEventsByDisplayDate );

   return eventDisplays;
}


/** PUBLIC
*
*   CalendarEventDataSource/getEventsForWeek.
*
* PARAMETERS
*      date     - Date object, uses the month and year and date  to get all events 
*                 for the given day.
* RETURN
*      array    - of events for the day
*/


CalendarEventDataSource.prototype.getEventsForWeek = function calEvent_getEventsForWeek( date )
{
   var eventDisplays =  new Array();

   var eventList = this.gICalLib.getEventsForWeek( date );
   
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEventDisplay);
      
      var EventObject = new Object;
      
      EventObject.event = tmpevent.event;
      
      EventObject.displayDate = tmpevent.displayDate;
      EventObject.displayEndDate = tmpevent.displayEndDate;

      eventDisplays[ eventDisplays.length ] = EventObject;
   }

   eventDisplays.sort( this.orderEventsByDisplayDate );

   return eventDisplays;
}

/** PUBLIC
*
*   CalendarEventDataSource/getEventsForMonth.
*
* PARAMETERS
*      date     - Date object, uses the month and year to get all events 
*                 for the given month.
* RETURN
*      array    - of events for the month
*/

CalendarEventDataSource.prototype.getEventsForMonth = function calEvent_getEventsForMonth( date )
{
   var eventDisplays =  new Array();

   var eventList = this.gICalLib.getEventsForMonth( date );
   
   while( eventList.hasMoreElements() )
   {
      eventDisplays[ eventDisplays.length ] = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEventDisplay);
   }
   
   eventDisplays.sort( this.orderEventsByDisplayDate );

   return eventDisplays;
}
/** PRIVATE
*
*   CalendarEventDataSource/getEventsDisplayForRange.
*
* PARAMETERS
*      startdate     - Date object, startdate 
*      enddate     - Date object, enddate 
*                .
* RETURN
*      array    - of events for the month
*/

CalendarEventDataSource.prototype.getEventsDisplayForRange = function calEvent_getEventsDisplayForRange( startdate,enddate )
{
   var eventDisplays =  new Array();

   var eventList = this.gICalLib.getEventsForRange( startdate, enddate);
   
   while( eventList.hasMoreElements() )
   {
      eventDisplays[ eventDisplays.length ] = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEventDisplay);
   }

   eventDisplays.sort( this.orderEventsByDisplayDate );

   return eventDisplays;
}

/** PUBLIC
*
*   CalendarEventDataSource/getNextEvents.
*
* PARAMETERS
*      EventsToGet   - The number of events to return 
*
* RETURN
*      array    - of the next "EventsToGet" events
*/
CalendarEventDataSource.prototype.getNextEvents = function calEvent_getNextEvents( EventsToGet )
{
   var eventDisplays =  new Array();
   
   var today = new Date();
   
   var eventList = this.gICalLib.getNextNEvents( today, EventsToGet );
   
   while( eventList.hasMoreElements() )
   {
      eventDisplays[ eventDisplays.length ] = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEventDisplay);;
   }
   eventDisplays.sort( this.orderRawEventsByDate );

   return eventDisplays;
}


/** PUBLIC
*
*   CalendarEventDataSource/getAllEvents.
*
* RETURN
*      array    - of ALL events
*/

CalendarEventDataSource.prototype.getAllEvents = function calEvent_getAllEvents( )
{
   // clone the array in case the caller messes with it
   
   var eventList = this.gICalLib.getAllEvents();

   this.currentEvents = new Array();
   
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      
      this.currentEvents.push( tmpevent );
   }
   this.currentEvents.sort( this.orderRawEventsByDate );

   return this.currentEvents;
}

CalendarEventDataSource.prototype.getEventsForRange = function calEvent_getEventsForRange( StartDate, EndDate )
{
  //dump( "\n->get events from "+StartDate+"\n"+EndDate );
   var eventList = this.gICalLib.getFirstEventsForRange( StartDate, EndDate );
   
   this.currentEvents = new Array();
   
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      dump( "\n->event is "+tmpevent );
      this.currentEvents.push( tmpevent );
   }
   this.currentEvents.sort( this.orderRawEventsByDate );

   return this.currentEvents;
}

CalendarEventDataSource.prototype.getAllFutureEvents = function calEvent_getAllFutureEvents()
{
   var Today = new Date();

   //do this to allow all day events to show up all day long
   var Start = new Date( Today.getFullYear(), Today.getMonth(), Today.getDate(), 0, 0, 0 );
   
   var Infinity = new Date( Today.getFullYear()+100, 31, 11 );

   var eventList = this.gICalLib.getFirstEventsForRange( Start, Infinity );
   
   this.currentEvents = new Array();
   
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      
      this.currentEvents.push ( tmpevent );
   }
   this.currentEvents.sort( this.orderRawEventsByDate );

   return this.currentEvents;
}

CalendarEventDataSource.prototype.getICalLib = function calEvent_getICalLib()
{
   return this.gICalLib;
}

/** PUBLIC
*
*   CalendarEventDataSource/makeNewEvent.
*
* RETURN
*      new event, not SAVED yet, use addEvent to save it.
*/

CalendarEventDataSource.prototype.makeNewEvent = function calEvent_makeNewEvent( date )
{
   var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
   var iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
   
   if( date )
   {
       iCalEvent.start.setTime( date );
   }
   
   return iCalEvent;
}


/* TO DO STUFF */
/* getAllToDos() 
 * return all todos
 *
 *
 */
CalendarEventDataSource.prototype.getAllToDos = function calEvent_getAllToDos()
{
   var eventList = this.gICalLib.getAllTodos( );
   
   var eventArray = new Array();
   
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalTodo);
      
      eventArray[ eventArray.length ] = tmpevent;
   }
   eventArray.sort( this.orderToDosByDueDate );

   return eventArray;
}
/* getToDosForRange() 
 * getToDosForRange 
 * return the ToDos which due date is in the given date range
 * return only the completed ones if the unifindertodo checkbox 
 * is true
 */
CalendarEventDataSource.prototype.getToDosForRange = function calEvent_getToDosForRange( StartDate, EndDate )
{
   var Checked = document.getElementById( "only-completed-checkbox" ).checked;
   
   gICalLib.resetFilter();
      
   if( Checked === true )
      gICalLib.filter.completed.setTime( new Date() );
   
   var eventList = this.gICalLib.getAllTodos( );
   
   var eventArray = new Array();
   var EndDatePlusOne = new Date(EndDate.getFullYear(),EndDate.getMonth(),EndDate.getDate()+1) ;
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalTodo);
      var dueTime = tmpevent.due.getTime() ;
     
      if( (dueTime  >= StartDate.getTime()) &&  (dueTime < EndDatePlusOne.getTime()) )
	{
	  eventArray[ eventArray.length ] = tmpevent;
	}
   }
   eventArray.sort( this.orderToDosByDueDate );
   return eventArray;
}

/** PACKAGE STATIC
*   CalendarEvent orderToDosByDueDate.
* 
* NOTES
*   Used to sort todo table by date
*/

CalendarEventDataSource.prototype.orderToDosByDueDate = function calEvent_orderToDosByDueDate( toDoA, toDoB )
{
   if( ( toDoA.due.getTime() - toDoB.due.getTime() ) == 0 ) 
   {
      return( toDoA.start.getTime() - toDoB.start.getTime() );
   }
   return( toDoA.due.getTime() - toDoB.due.getTime() );
}


/** PACKAGE STATIC
*   CalendarEvent orderEventsByDisplayDate.
* 
* NOTES
*   Used to sort events table by date
*/

CalendarEventDataSource.prototype.orderEventsByDisplayDate = function calEvent_orderEventsByDisplayDate( eventA, eventB )
{
   var r=eventA.displayDate - eventB.displayDate;
   if (r==0) {
     var titleA = eventTitleOrEmpty(eventA);
     var titleB = eventTitleOrEmpty(eventB);
     return ( titleA < titleB ? -1 : 
              titleA > titleB ?  1 : 0);
   }
   return(r);
}

function eventTitleOrEmpty(event) {
  return ("title" in event && event.title != null) ? event.title : "";
}

/** PACKAGE STATIC
*   CalendarEvent orderRawEventsByDate.
* 
* NOTES
*   Used to sort events table by date
*/

CalendarEventDataSource.prototype.orderRawEventsByDate = function calEvent_orderRawEventsByDate( eventA, eventB )
{
    return( getCurrentNextOrPreviousRecurrence( eventA ).getTime() - getCurrentNextOrPreviousRecurrence( eventB ).getTime() );
}

/** If now is during an occurrence, return the ocurrence.
    Else if now is before an ocurrence, return the next ocurrence.
    Otherwise return the previous ocurrence. **/
function getCurrentNextOrPreviousRecurrence( calendarEvent )
{
   var isValid = false;

   var eventStartDate;

   if( calendarEvent.recur )
   {
      var now = new Date();

      var result = new Object();

      var dur = calendarEvent.end.getTime() - calendarEvent.start.getTime();

      // To find current event when now is during event, look for occurrence
      // starting duration ago.
      var probeTime = now.getTime() - dur;

      isValid = calendarEvent.getNextRecurrence( probeTime, result );

      if( isValid )
      {
         eventStartDate = new Date( result.value );
      }
      else
      {
         isValid = calendarEvent.getPreviousOccurrence( probeTime, result );
         
         if (isValid)
         {
            eventStartDate = new Date( result.value );
         }
      }
   }
   
   if( !isValid )
   {
      eventStartDate = new Date( calendarEvent.start.getTime() );
   }
      
   return eventStartDate;
}

/******************************************************************************************************
******************************************************************************************************

ALARM RELATED CODE

******************************************************************************************************
*******************************************************************************************************/
CalendarEventDataSource.prototype.prepareAlarms = function calEvent_prepareAlarms( )
{
    this.alarmObserver =  new CalendarAlarmObserver( this );
    
    this.gICalLib.addObserver( this.alarmObserver );
}

function CalendarAlarmObserver( calendarService )
{
    this.pendingAlarmList = new Array();
    this.addToPending = true;
    this.calendarService = calendarService;
}

CalendarAlarmObserver.prototype.firePendingAlarms = function calAlarm_firePendingAlarms( observer )
{
    this.addToPending = false;
    
    if( this.pendingAlarmList )
    {
      for( var i in this.pendingAlarmList )
      {
         this.fireAlarm( this.pendingAlarmList[ i ] );
      }
    }
   this.pendingAlarmList = null;
    
    
}

CalendarAlarmObserver.prototype.onStartBatch = function(){}
CalendarAlarmObserver.prototype.onEndBatch = function(){}
CalendarAlarmObserver.prototype.onLoad = function( calendarEvent ){}
CalendarAlarmObserver.prototype.onAddItem = function( calendarEvent ){}
CalendarAlarmObserver.prototype.onModifyItem = function( calendarEvent, originalEvent ){}
CalendarAlarmObserver.prototype.onDeleteItem = function( calendarEvent ){}
CalendarAlarmObserver.prototype.onError = function(){}

CalendarAlarmObserver.prototype.onAlarm = function calAlarm_onAlarm( calendarEvent )
{
    dump( "caEvent.alarmWentOff is "+ calendarEvent + "\n" );
    
    if( this.addToPending )
    {
        dump( "defering alarm "+ calendarEvent + "\n" );
        this.pendingAlarmList.push( calendarEvent );
    }
    else
    {
        this.fireAlarm( calendarEvent )
    }
}

CalendarAlarmObserver.prototype.fireAlarm = function calAlarm_fireAlarm( calendarEvent )
{
   dump( "Fire alarm "+ calendarEvent + "\n" );
   
   if( typeof( gCalendarWindow ) == "undefined" )
   {
      return;
   }
      
   if( getBoolPref(gCalendarWindow.calendarPreferences.calendarPref, 
		   "alarms.playsound", 
		   gCalendarBundle.getString("playAlarmSound") ) 
       )
   {
      playSound();
   }
   
   addEventToDialog( calendarEvent );
   
   if ( calendarEvent.alarmEmailAddress )
   {
     var EmailBody = gCalendarEmailBundle.getFormattedString("AlarmEmailBody", [calendarEvent.title, calendarEvent.start.toString()]);
     var EmailSubject = gCalendarEmailBundle.getFormattedString("AlarmEmailSubject", [calendarEvent.title]);
         
      //send an email for the event
      sendEmail( EmailSubject, EmailBody, calendarEvent.alarmEmailAddress, null, null, null, null );
   }
}
