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
                     };
                     
                     These methods are now called synchronously, if you add an event the onAddItem
                     method will be called during your call to add the event.

*
* NOTES
*   Is a singleton, the first invocation creates an instance, subsequent calls return the same instance.
*/

function CalendarEventDataSource( observer, UserPath, syncPath )
{
      var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
            
      this.gICalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
        
      /*
      ** FROM HERE TO "<<HERE" IS A HACK FROM JSLIB, REPLACE THIS IF YOU KNOW HOW
      */
      const JS_DIR_UTILS_FILE_DIR_CID             = "@mozilla.org/file/directory_service;1";
      
      const JS_DIR_UTILS_I_PROPS                  = "nsIProperties";
      
      const JS_DIR_UTILS_DIR                  = new Components.Constructor(
                                                JS_DIR_UTILS_FILE_DIR_CID,
                                                JS_DIR_UTILS_I_PROPS);
      
      const JS_DIR_UTILS_NSIFILE                  = Components.interfaces.nsIFile;
      
      var rv;

      try { rv=(new JS_DIR_UTILS_DIR()).get("ProfD", JS_DIR_UTILS_NSIFILE).path; }
      
      catch (e)
      {
       dump("ERROR! IN CALENDAR EVENT.JS");
       rv=null;
      }
      
      /*
      ** <<HERE
      */


      this.UserPath = rv;

      this.gICalLib.setServer( this.UserPath+"/CalendarDataFile.txt" );
      
      this.gICalLib.addObserver( observer );
      
      this.prepareAlarms( ); 
}



CalendarEventDataSource.InitService = function( root )
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

CalendarEventDataSource.prototype.onServiceStartup = function( root )
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

CalendarEventDataSource.prototype.search = function( searchText, fieldName )
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
      var eventTable = this.getAllEvents();
      
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
               var objValue = calendarEvent[ fieldName[i] ].toLowerCase();
         
               if( objValue )
               {
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
   return searchEventTable;
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


CalendarEventDataSource.prototype.getEventsForDay = function( date )
{
   var eventDisplays =  new Array();

   var displayDates =  new Object();
   var eventList = this.gICalLib.getEventsForDay( date, displayDates );
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      
      var displayDate = new Date( displayDates.value.getNext().QueryInterface(Components.interfaces.nsISupportsPRTime).data );
      
      var EventObject = new Object;
      
      EventObject.event = tmpevent;
      
      EventObject.displayDate = displayDate;
      
      eventDisplays[ eventDisplays.length ] = EventObject;
   }

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


CalendarEventDataSource.prototype.getEventsForWeek = function( date )
{
   var eventDisplays =  new Array();

   var displayDates =  new Object();
   var eventList = this.gICalLib.getEventsForWeek( date, displayDates );
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      
      var displayDate = new Date( displayDates.value.getNext().QueryInterface(Components.interfaces.nsISupportsPRTime).data );
      
      var EventObject = new Object;
      
      EventObject.event = tmpevent;
      
      EventObject.displayDate = displayDate;
      
      eventDisplays[ eventDisplays.length ] = EventObject;
   }

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

CalendarEventDataSource.prototype.getEventsForMonth = function( date )
{
   var eventDisplays =  new Array();

   var displayDates =  new Object();
   var eventList = this.gICalLib.getEventsForMonth( date, displayDates );
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      
      var displayDate = new Date( displayDates.value.getNext().QueryInterface(Components.interfaces.nsISupportsPRTime).data );
      
      var EventObject = new Object;
      
      EventObject.event = tmpevent;
      
      EventObject.displayDate = displayDate;
      
      eventDisplays[ eventDisplays.length ] = EventObject;
   }

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

CalendarEventDataSource.prototype.getNextEvents = function( EventsToGet )
{
   var eventDisplays =  new Array();

   
   var today = new Date();
   var displayDates =  new Object();
   var eventList = this.gICalLib.getNextNEvents( today, EventsToGet, displayDates );
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      
      var displayDate = new Date( displayDates.value.getNext().QueryInterface(Components.interfaces.nsISupportsPRTime).data );
      
      var EventObject = new Object;
      
      EventObject.event = tmpevent;
      
      EventObject.displayDate = displayDate;
      
      eventDisplays[ eventDisplays.length ] = EventObject;
   }

   return eventDisplays;
}


/** PUBLIC
*
*   CalendarEventDataSource/getAllEvents.
*
* RETURN
*      array    - of ALL events
*/

CalendarEventDataSource.prototype.getAllEvents = function( )
{
   // clone the array in case the caller messes with it
   
   var eventList = this.gICalLib.getAllEvents();

   var eventArray = new Array();
   
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      eventArray[ eventArray.length ] = tmpevent;
   }
   return eventArray;
}

/** PUBLIC
*
*   CalendarEventDataSource/makeNewEvent.
*
* RETURN
*      new event, not SAVED yet, use addEvent to save it.
*/

CalendarEventDataSource.prototype.makeNewEvent = function( date )
{
   var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
   var iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
   
   if( date )
   {
       iCalEvent.start.setTime( date );
   }
   
   return iCalEvent;
}

CalendarEventDataSource.prototype.getICalLib = function()
{
   return this.gICalLib;
}

/*
** Start time and end time are optional.  
*/

CalendarEventDataSource.prototype.openNewEventDialog = function( onOK, startTime, endTime )
{
   var args = new Object();
   
   var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
   args.calendarEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
   
   args.mode = "new";
   
   args.onOk =  onOK; 
   
   if( !startTime )
      args.calendarEvent.start.setTime( new Date() );
   else
      args.calendarEvent.start.setTime( startTime );

   if( !endTime )
   {
      args.calendarEvent.end.setTime( new Date() );

      args.calendarEvent.end.hour++;
   }
   else
      args.calendarEvent.end.setTime( endTime );
   
   //this doens't work yet
   calendar.openDialog("caNewEvent", "chrome://calendar/content/ca-event-dialog.xul", false, args );   
}

/******************************************************************************************************
******************************************************************************************************

ALARM RELATED CODE

******************************************************************************************************
*******************************************************************************************************/


CalendarEventDataSource.prototype.launchAlarmDialog = function( Event )
{
   //var args = new Object();

   //args.calendarEvent = Event;

   //openDialog( "caAlarmDialog", "chrome://calendar/content/ca-event-alert-dialog.xul", false, args );   
}


CalendarEventDataSource.prototype.checkAlarmDialog = function( )
{
   //var AlarmDialogIsOpen = Root.getRootWindowAppPath( "controlbar" ).penapplication.getDialogPath( "caAlarmDialog" ); //change this to do something
   
   //if( AlarmDialogIsOpen )
   //   return( true );
   //else
   //   return( false );

}

CalendarEventDataSource.prototype.addEventToDialog = function( Event )
{
   //if( this.checkAlarmDialog() )
   //{
   //   var DialogWindow = Root.getRootWindowAppPath( "controlbar" ).penapplication.getDialogPath( "caAlarmDialog" );

   //   DialogWindow.createAlarmBox( Event );
   //}
   //else
   //{
   //   this.launchAlarmDialog( Event );
   //}
}



CalendarEventDataSource.prototype.makeXmlNode = function( xmlDocument, calendarEvent )
{
    
    var doDate = function( node, name, icaldate )
    {
        var jsDate = new Date( icaldate.getTime() );
        
        node.setAttribute( name,  jsDate.toString() );
        node.setAttribute( name + "Year",   jsDate.getFullYear() );
        node.setAttribute( name + "Month",  jsDate.getMonth() + 1);
        node.setAttribute( name + "Day",    jsDate.getDate() );
        node.setAttribute( name + "Hour",   jsDate.getHours() );
        node.setAttribute( name + "Minute", jsDate.getMinutes() );
    }
    
    var checkString = function( str )
    {
        if( typeof( str ) == "string" )
            return str;
        else
            return ""
    }
    
    var checkNumber = function( num )
    {
        if( typeof( num ) == "undefined" || num == null )
            return "";
        else
            return num
    }
    
    var checkBoolean = function( bool )
    {
        if( bool == "false")      
            return "false"
        else if( bool )      // this is false for: false, 0, undefined, null, ""
            return "true";
        else
            return "false"
    }
    
    // make the event tag
    var eventNode = xmlDocument.createElement( "event" );

    eventNode.setAttribute( "id",               calendarEvent.id );
    eventNode.setAttribute( "syncId",           calendarEvent.syncId );
    
    doDate( eventNode, "start", calendarEvent.start );
    doDate( eventNode, "end", calendarEvent.end );
    
    eventNode.setAttribute( "allDay",           checkBoolean( calendarEvent.allDay ) );
    
    eventNode.setAttribute( "title",            checkString( calendarEvent.title ) );
    eventNode.setAttribute( "description",      checkString( calendarEvent.description ) );
    eventNode.setAttribute( "category",         checkString( calendarEvent.category ) );
    eventNode.setAttribute( "location",         checkString( calendarEvent.location ) );
    eventNode.setAttribute( "privateEvent",     checkBoolean( calendarEvent.privateEvent ) );
    
    eventNode.setAttribute( "inviteEmailAddress", checkString( calendarEvent.inviteEmailAddress ) );
    
    eventNode.setAttribute( "alarm",            checkBoolean( calendarEvent.alarm ) );
    eventNode.setAttribute( "alarmLength",      checkNumber( calendarEvent.alarmLength ) );
    eventNode.setAttribute( "alarmUnits",       checkString( calendarEvent.alarmUnits ) );  
    eventNode.setAttribute( "alarmEmailAddress",checkString( calendarEvent.alarmEmailAddress ) );
    
    eventNode.setAttribute( "recur",            checkBoolean( calendarEvent.recur ) );
    eventNode.setAttribute( "recurUnits",       checkString( calendarEvent.recurUnits ) ); 
    eventNode.setAttribute( "recurForever",     checkBoolean( calendarEvent.recurForever ) );
    eventNode.setAttribute( "recurInterval",    checkNumber( calendarEvent.recurInterval ) );
    eventNode.setAttribute( "recurWeekdays",    checkNumber( calendarEvent.recurWeekdays ) );
    eventNode.setAttribute( "recurWeekNumber",  checkNumber( calendarEvent.recurWeekNumber ) );
    
    doDate( eventNode, "recurEnd", calendarEvent.recurEnd );
   
    return eventNode;
}

CalendarEventDataSource.prototype.prepareAlarms = function( )
{
    this.alarmObserver =  new CalendarAlarmObserver( this );
    
    this.gICalLib.addObserver( this.alarmObserver );
    
}

function CalendarAlarmObserver( calendarService )
{
   this.calendarService = calendarService;
}
CalendarAlarmObserver.prototype.onStartBatch = function()
{
}

CalendarAlarmObserver.prototype.onEndBatch = function()
{
}

CalendarAlarmObserver.prototype.onLoad = function( calendarEvent )
{
}

CalendarAlarmObserver.prototype.onAddItem = function( calendarEvent )
{
}

CalendarAlarmObserver.prototype.onModifyItem = function( calendarEvent, originalEvent )
{
}

CalendarAlarmObserver.prototype.onDeleteItem = function( calendarEvent )
{
}


CalendarAlarmObserver.prototype.onAlarm = function( calendarEvent )
{
   debug( "caEvent.alarmWentOff is "+ calendarEvent );
         
   this.calendarService.addEventToDialog( calendarEvent );

   if ( calendarEvent.alarmEmailAddress )
   {
      
      //debug( "about to send an email to "+ calendarEvent.alarmEmailAddress + "with subject "+ calendarEvent.title );
      
      var emailService = Root.getRootWindowAppPath( "controlbar" ).penapplication.getService( "org.penzilla.email" );
        
      if( emailService )
      {
         var EmailBody = "Calendar Event Alarm Went Off!\n----------------------------\n";
         EmailBody += "Title: "+calendarEvent.title + " at " + calendarEvent.start.toString() +  "\n";
         EmailBody += "This message sent to you from the OECalendar.\nhttp://www.oeone.com";

         emailService.sendEmail( 'Calendar Event', EmailBody, calendarEvent.alarmEmailAddress );
      }
   }
}

function debug(str )
{
    dump( "\n CalendarEvent.js DEBUG: "+ str + "\n");
}

