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
        
      if( UserPath == null )
         this.UserPath = penapplication.penroot.getUserPath();
      else
         this.UserPath = UserPath;

      this.gICalLib.setServer( this.UserPath+"/.oecalendar" );
      
      this.gICalLib.addObserver( observer );
      
      this.prepareSync( syncPath );
              
      this.prepareAlarms( ); 

      this.timers = new Array();
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
    // if there is a a palm service register with it so we get notified 
    // of palm sync events
    
    this.palmService = root.servicesManager.getServiceByName( "org.penzilla.palmsync" );
    
    if( this.palmService )
    {
        this.palmService.addObserver( this );
    }

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
   var args = new Object();

   args.calendarEvent = Event;

   Root.getRootWindowAppPath( "controlbar" ).penapplication.openDialog( "caAlarmDialog", "chrome://calendar/content/ca-event-alert-dialog.xul", false, args );   
}


CalendarEventDataSource.prototype.checkAlarmDialog = function( )
{
   var AlarmDialogIsOpen = Root.getRootWindowAppPath( "controlbar" ).penapplication.getDialogPath( "caAlarmDialog" ); //change this to do something
   
   if( AlarmDialogIsOpen )
      return( true );
   else
      return( false );

}

CalendarEventDataSource.prototype.addEventToDialog = function( Event )
{
   if( this.checkAlarmDialog() )
   {
      var DialogWindow = Root.getRootWindowAppPath( "controlbar" ).penapplication.getDialogPath( "caAlarmDialog" );

      DialogWindow.createAlarmBox( Event );
   }
   else
   {
      this.launchAlarmDialog( Event );
   }
}




/** PRIVATE
*
*   CalendarEventDataSource/setUpAlarmTimers.
*
*      create timers for each event that has an alarm that hasn't gone off.
*/
/*
CalendarEventDataSource.prototype.setUpAlarmTimers = function( )
{
   for( i = 0; i < this.timers.length; i++ )
   {
      window.clearTimeout( this.timers[i] );
   }

   var Today = new Date( );

   var eventList = gICalLib.searchAlarm( Today.getFullYear(), ( Today.getMonth() + 1 ), Today.getDate(), Today.getHours(), Today.getMinutes() );

   this.alarmTable = Array();
   while( eventList.hasMoreElements() )
   {
      var tmpevent = eventList.getNext().QueryInterface(Components.interfaces.oeIICalEvent);
      alarmTable[ alarmTable.length ] =  tmpevent;
   }

   // sort them by date, we keep them that way
   
   this.alarmTable.sort( CalendarEventDataSource.orderEventsByDate );

   for( i = 0; i < this.alarmTable.length; i++ )
   {
      AlarmTimeInMS = this.alarmTable[i].calculateAlarmTime( );
      
      var Now = new Date();

      var TimeToAlarmInMS = AlarmTimeInMS - Now.getTime();
      
      this.timers[ this.timers.length ] = setTimeout( "CalendarEventDataSource.respondAlarmTimeout( "+this.alarmTable[i].id+" )", TimeToAlarmInMS );

   }
   
   //alarm timers return only the alarms for the next 24 hours.  Set a timeout to check again at that time.
   if( this.checkAlarmIn24HoursTimeout )
      clearTimeout( this.checkAlarmIn24HoursTimeout );


   this.checkAlarmIn24HoursTimeout = setTimeout( "CalendarEventDataSource.setUpAlarmTimers()", 1000 * 60 * 60 * 24 );
} 
*/
/*      
CalendarEventDataSource.prototype.respondAcknowledgeAlarm = function( calendarEvent )
{
   // notify observer
   
   for( var index in this.observerList )
   {
      var observer = this.observerList[ index ];
      
      if( observer.onAcknowledgeAlarm )
      {
         observer.onAcknowledgeAlarm( calendarEvent );
      }
   }
}
*/
/** PRIVATE
*
*   CalendarEventDataSource/respondAlarmEvent.
*
*      Response method for an alarm going off
*/
/*
CalendarEventDataSource.respondAlarmTimeout = function( calendarEventID )
//function respondAlarmTimeout( calendarEventID )
{
   var calendarEvent = gICalLib.fetchEvent( calendarEventID );
   
   //if( calendarEvent && calendarEvent.checkAlarm() )
   //{
      debug( "in CalendarEventDataSource.prototype.respondAlarmEvent\n+++++++++ALARM WENT OFF \n" + 
                     calendarEvent.toSource() + 
                     "\n at " + 
                     calendarEvent.start.toString()
                     + "\n+++++++++\n" );
      
      // :TODO: The email sending stuff should be part of the CalendarEvent class
      
      if ( calendarEvent.alarmEmailAddress )
      {
         
         //debug( "about to send an email to "+ calendarEvent.alarmEmailAddress + "with subject "+ calendarEvent.title );
         
         //calendar.sendEmail( "Calendar Event", 
        //                     "Title: "+ calendarEvent.title + " @ "+ calendarEvent.start.toString() + "\nThis message sent from OECalendar.", 
        //                     calendarEvent.alarmEmailAddress );
      }
   

      for( var index in gCalendarEventDataSource.observerList )
      {
         var observer = gCalendarEventDataSource.observerList[ index ];
         
         if( observer.onAlarm )
         {
            observer.onAlarm( calendarEvent );
         }
      }
      
   //}
   //else
   //{
      // reset the timer if required
      
   //   if( calendarEvent )
   //      calendarEvent.setUpAlarmTimer( this );
   //}
}
*/
/** PRIVATE
* 
* NOTES
*        Helper function for setting alarms
* PARAMETERS
*        minBefore - number of minutes BEFORE the event that the alarm is to be set for.
*        update    - if true the datasource is updated right away.
*/

/*
CalendarAlarm.prototype.setAlarm = function( units, timeBefore, update )
{
   if( this.alarmWentOff != false || 
       this.alarm != true ||
       this.alarmLength != timeBefore ||
       this.alarmUnits != units )
   {
      this.alarmWentOff = false;
      this.alarm = true;
      this.alarmLength = timeBefore;
      this.alarmUnits = units;
   
      if( this.dataSource && update )
      {
         this.dataSource.modifyEvent( this );
      }
   }
}   */

/** PUBLIC
* 
* PARAMETERS
*
* RETURN 
*         true if the alarm for this event should be sounded
*/

/*
CalendarAlarm.prototype.checkAlarm = function( )
{
   if( this.alarm && ( !this.alarmWentOff || ( this.alarmWentOff && this.snoozeTime ) ) )
   {
      var alarmTimeMS = this.event.calculateAlarmTime(); 
      
      var eventTimeMS = this.start.getTime(); 
      
      var now = new Date();
      
      var nowMS = now.getTime();
      
      if( ( nowMS > alarmTimeMS && nowMS < eventTimeMS ) || this.snoozeTime )
      {
         return true;
      }
      else
      {
         return false;
      }
   }
   else
      return false;
}*/

/** PRIVATE
* 
* PARAMETERS
*
* RETURN 
*         clear the alarm timer if there is one
*/

/*
CalendarAlarm.prototype.clearAlarmTimer = function( )
{
   if( this.timerID )
   {
      debug( "clearing alarm for :" + this.id );
      
      window.clearTimeout( this.timerID  );
      this.timerID = null;
   }
}*/

/** PRIVATE
* 
* PARAMETERS
*
* RETURN 
*         true if the alarm for this event should be sounded
*/

/*
CalendarAlarm.prototype.setUpAlarmTimer = function( dataSource )
{
   // remove any existing timer
   
   this.clearAlarmTimer();
  
   // set up the new timer
   debug( "this.alarm is "+this.alarm );

   if( this.alarm  && ( !this.alarmWentOff || ( this.alarmWentOff && this.snoozeTime ) ) )
   {
      debug( "testing alarm for :" + this.title + "  " + this.start );
      
      var alarmTimeMS = this.event.calculateAlarmTime(); 
      
      var now = new Date();
      var nowMS = now.getTime();
      
      debug( "eventTime: " + this.start );
      debug( "alarmTime: " + new Date(alarmTimeMS) );
      debug( "now :" + new Date(nowMS) );
      debug( "nowMS :\n" + nowMS + " this.start.getTime() is \n"+this.start.getTime() );
      debug( "snoozeTime is "+this.snoozeTime );
      if( this.snoozeTime )
      {
         var timeTillAlarm = this.snoozeTime - nowMS;
         
         debug( "!-- SNOOZE IS TRUE! nowMS < this.start in penCalendarEvent.js, timeTillAlarm is "+ timeTillAlarm +" \n" );

         if( timeTillAlarm < 10 )
         {
            timeTillAlarm = 10;
         }
          
         debug( "\n Setting alarm for MS:" + timeTillAlarm );
         
         this.timerID = window.setTimeout( "CalendarEventDataSource.respondAlarmTimeout(" + this.id + ")", timeTillAlarm  );


      }
      else if( nowMS < this.start.getTime() && !this.snoozeTime )
      {
         var timeTillAlarm = alarmTimeMS - nowMS;
         
         debug( "!-- NO SNOOZE! nowMS < this.start in penCalendarEvent.js, timeTillAlarm is "+ timeTillAlarm +" \n" );

         if( timeTillAlarm < 10 )
         {
            timeTillAlarm = 10;
         }
          
         debug( "\n Setting alarm for MS:" + timeTillAlarm );
         
         this.timerID = window.setTimeout( "CalendarEventDataSource.respondAlarmTimeout(" + this.id + ")", timeTillAlarm  );

      }
   }
}*/


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

CalendarEventDataSource.prototype.fillEventFromXmlNode = function( calendarEvent, eventNode )
{
    
    var checkDate = function( node, name )
    {
        var year    = Number( node.getAttribute( name + "Year" ) );
        var month   = Number( node.getAttribute( name + "Month" ) );
        var day     = Number( node.getAttribute( name + "Day" ) );
        var hour    = Number( node.getAttribute( name + "Hour" ) );
        var minute  = Number( node.getAttribute( name + "Minute" ) );
        
        var jsDate = new Date( year, month - 1, day, hour, minute, 0, 0  );
        
        return jsDate.getTime();
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
            return 0;
        else
            return num
    }
    
    var checkBoolean = function( bool )
    {
        if( bool == "false")      
            return false
        else if( bool )      // this is false for: false, 0, undefined, null, ""
            return true;
        else
            return false
    }
        
    calendarEvent.syncId = checkNumber(  eventNode.getAttribute( "syncId" ) );
    
    calendarEvent.start.setTime( checkDate( eventNode, "start" ) );
    calendarEvent.end.setTime( checkDate( eventNode, "end" ) );
    
    calendarEvent.allDay       = checkBoolean( eventNode.getAttribute( "allDay"   ) );
    
    calendarEvent.title        = checkString(  eventNode.getAttribute( "title"   ) );
    calendarEvent.description  = checkString(  eventNode.getAttribute( "description"   ) );
    calendarEvent.category     = checkString(  eventNode.getAttribute( "category"   ) );
    calendarEvent.location     = checkString(  eventNode.getAttribute( "location"   ) );
    calendarEvent.privateEvent = checkBoolean( eventNode.getAttribute( "privateEvent"   ) );
    
    calendarEvent.inviteEmailAddress = checkString(  eventNode.getAttribute( "inviteEmailAddress"   ) );
    
    calendarEvent.alarm             = checkBoolean( eventNode.getAttribute( "alarm"   ) );
    calendarEvent.alarmLength       = checkNumber(  eventNode.getAttribute( "alarmLength"   ) );
    calendarEvent.alarmUnits        = checkString(  eventNode.getAttribute( "alarmUnits"   ) );
    calendarEvent.alarmEmailAddress = checkString(  eventNode.getAttribute( "alarmEmailAddress"   ) );
    
    calendarEvent.recur           = checkBoolean( eventNode.getAttribute( "recur"   ) );
    calendarEvent.recurUnits      = checkString(  eventNode.getAttribute( "recurUnits"   ) );
    calendarEvent.recurForever    = checkBoolean( eventNode.getAttribute( "recurForever"   ) );
    calendarEvent.recurInterval   = checkNumber(  eventNode.getAttribute( "recurInterval"   ) );
    calendarEvent.recurWeekdays   = checkNumber(  eventNode.getAttribute( "recurWeekdays"   ) );
    calendarEvent.recurWeekNumber = checkNumber(  eventNode.getAttribute( "recurWeekNumber"   ) );
   
    calendarEvent.recurEnd.setTime( checkDate( eventNode, "recurEnd" ) );
    
    return calendarEvent;
}


/** PUBLIC
* 
* RETURN
*    An xml document with all the event info
*/

CalendarEventDataSource.prototype.makeXmlDocument = function( eventList )
{
    // use the domparser to create the XML 
    var domParser = Components.classes["@mozilla.org/xmlextras/domparser;1"].getService( Components.interfaces.nsIDOMParser );
    
    // start with one tag
    var xmlDocument = domParser.parseFromString( "<events/>", "text/xml" );
    
    // get the top tag, there will only be one.
    var topNodeList = xmlDocument.getElementsByTagName( "events" );
    var topNode = topNodeList[0];
    
    
    // add each event as an element
    
    for( var index = 0; index < eventList.length; ++index )
    {
        var calendarEvent = eventList[ index ];
        
        var eventNode = this.makeXmlNode( xmlDocument, calendarEvent );
        
        topNode.appendChild( eventNode );
    }

    // return the document
    
    return xmlDocument;
}

/** PRIVATE
* 
*    Before doing any sync ing we make sure the 
*    directory is there, if it isn't, we generate and
*    return false meaning don't bother 
*/

CalendarEventDataSource.prototype.prepareSyncDirectory = function( )
{
    // make sure the directory exists
  
    var dir = new Dir( this.syncPath );
 
    if( !dir.exists() )
    {
        var permissions = 0744;
        dir.create();
        dir.permissions = permissions;
        
        if( dir.exists() )
        {
            this.writeXmlDirectory(  this.syncPath );
        }
    }
    
    return  dir.exists();
}
    
/** PUBLIC
* 
* RETURN
*    An xml document with all the event info
*/

CalendarEventDataSource.prototype.writeXmlDirectory = function( path )
{
    // make sure the path has a "/" on the end
    
    if( path[ path.length - 1 ] != "/" )
    {
        path = path + "/";
    }
    
    // get all the events
    
    var eventTable = this.getAllEvents();
    
    // Save a file for each event, unless the existing file's content matches
    
    for( var index = 0; index < eventTable.length; ++index )
    {
        var calendarEvent = eventTable[ index ];
        
        // The file name has the event id in it
        var filePath = path + calendarEvent.id + ".xml";
        
        this.writeXmlFile( filePath, [ calendarEvent ] );
    }
}

/** PUBLIC
*    write an xml document with all the event info
*/

CalendarEventDataSource.prototype.writeXmlFile = function( path, eventList )
{
    var xmlDoc = this.makeXmlDocument( eventList );
    
    var domSerializer = Components.classes["@mozilla.org/xmlextras/xmlserializer;1"].getService( Components.interfaces.nsIDOMSerializer );
    var content = domSerializer.serializeToString( xmlDoc );
    
    penFileUtils.writeFile( path, content );
}


CalendarEventDataSource.prototype.prepareSync = function( syncPath )
{
    if( syncPath )
    {
        this.syncPath = syncPath+"/.oecalendarXmlDir/";
    }
    else
    {
        this.syncPath = this.UserPath+"/.oecalendarXmlDir/";
    }
    
    this.syncObserver =  new CalendarSyncObserver( this );
    
    this.gICalLib.addObserver( this.syncObserver );
    
}

CalendarEventDataSource.prototype.prepareAlarms = function( )
{
    this.alarmObserver =  new CalendarAlarmObserver( this );
    
    this.gICalLib.addObserver( this.alarmObserver );
    
}

CalendarEventDataSource.prototype.onPalmSyncStart = function(  )
{
    CalendarEventDataSource.debug( " CalendarEventDataSource START PALM SYNC");
    
    // get the sync directory ready if it is not there
    
    this.prepareSyncDirectory()
}
CalendarEventDataSource.prototype.onPalmSyncEnd = function( service, success )
{
    CalendarEventDataSource.debug( " CalendarEventDataSource END PALM SYNC");
    
    // update from synced files
    
    if( success )
    {
        var syncer = new DirectorySynchronizer();
        syncer.sync( this.syncPath, this );
    }
}



/** PUBLIC
* 
* RETURN
*    sync with an ical directory
*/
function DirectorySynchronizer(  )
{
}

DirectorySynchronizer.prototype.sync = function( path, calendarService )
{
    // make sure the path has a "/" on the end
    
    if( path[ path.length - 1 ] != "/" )
    {
        path = path + "/";
    }
    
    this.path = path;
    
    this.calendarService = calendarService;
    this.iCalLib = calendarService.getICalLib();
    this.eventArray = calendarService.getAllEvents();
    
    this.makeFileList();
    this.makeFileTable();
    this.makeAddArray( );
    this.makeDeleteUdateArrays();
    
    this.iCalLib.batchMode = true;
    this.addNewItems();
    this.updateItems();
    this.deleteItems();
    this.iCalLib.batchMode = false;

}

DirectorySynchronizer.prototype.makeFileList = function( )
{
    this.dir = new Dir( this.path );

    if( !this.dir.exists() )
    {
        throw "DirectorySynchronizer, unable to find sync dir: " + path;
    }
    
    this.fileList = this.dir.readDir();
}

DirectorySynchronizer.prototype.makeFileTable = function( )
{
    // Make a hash table by file name of the files in the list
    
    this.fileTable = new Object()
    
    for( var i in this.fileList  )
    {
        var file = this.fileList[i];
        var fileName = file.leaf;
        
        this.fileTable[ fileName ] = file;
    }
    
}   

DirectorySynchronizer.prototype.makeAddArray = function( )
{
    // Make a array of files that represent new events
    
    this.addArray = new Array()
    
    for( var i in this.fileList )
    {
        var file = this.fileList[i];
        var fileName = file.leaf;
        
        if( -1 != fileName.indexOf( "sync-file-palm" ) )
        {
            this.addArray.push( file );
        }
    }
}  

DirectorySynchronizer.prototype.makeDeleteUdateArrays = function( )
{
    // Find all events with matching file names,  they will be updated
    // Find all events without matching file names,  they will be deleted
    
    this.deleteArray = new Array();
    this.updateArray = new Array();
    
    for( var j = 0; j < this.eventArray.length; ++j )
    {
        var event = this.eventArray[ j ];
        var eventFileName = event.id + ".xml";
        
        if( ( eventFileName in this.fileTable ) )
        {
            this.updateArray.push( this.fileTable[ eventFileName ] );
        }
        else
        {
            this.deleteArray.push( event.id );
        }
    }
 }
 
DirectorySynchronizer.prototype.readFile = function( file )
{
    var content = "";
    
    var openOK = file.open( "r" );
    
    if( openOK )
    {
        content = file.read();
        file.close( );
    }
    
    return content
}

DirectorySynchronizer.prototype.makeXmlDoc = function( content )
{
    var domParser = Components.classes["@mozilla.org/xmlextras/domparser;1"].getService( Components.interfaces.nsIDOMParser );
    
    if( content )
    {
        try
        {
            var xmlDocument = domParser.parseFromString( content, "text/xml" );
        }
        catch( e )
        {
            dump( "DirectorySynchronizer: Unable to parse xml  '" + content + "'. " + e.toSource() ); 
        }
            
        if( xmlDocument )
        {
            return xmlDocument
        }
        else
        {
            dump( "DirectorySynchronizer: Unable to parse xml '" + content + "'. " ); 
        }
     }
     
     return null;
}


DirectorySynchronizer.prototype.addNewItems = function( )
{
    for( var i in this.addArray )
    {
        var file = this.addArray[i];
        
        // read the xml from the file and make an event from it.
        // delete the file so it won't bother us again
        
        var content = this.readFile( file );
        file.remove( true );
        
        var xmlDocument = this.makeXmlDoc( content );
        
        // convert xml document elements to event and add the event
        
        if( xmlDocument )
        {
            var eventElementList = xmlDocument.getElementsByTagName( "event" );
            
            for( var j = 0; j < eventElementList.length; ++j )
            {
                var eventElement = eventElementList[ j ];
                
                var newEvent = this.calendarService.makeNewEvent();
                this.calendarService.fillEventFromXmlNode( newEvent, eventElement );
                
                dump( "\n palm<<< sync add: " + file.path + "\n");
                this.iCalLib.addEvent( newEvent );
                
            }
        }
        else
        {
            dump( "DirectorySynchronizer: Unable to read xml from file '" + file.path + "'. " ); 
        }
    }
}  


DirectorySynchronizer.prototype.deleteItems = function( )
{
    for( var i in this.deleteArray )
    {
        var eventId = this.deleteArray[i];
        
        this.iCalLib.deleteEvent( eventId );
        CalendarEventDataSource.debug( "\n palm<<< sync delete: " + eventId + "\n");
    }
}  

 

DirectorySynchronizer.prototype.updateItems = function( )
{
    for( var i in this.updateArray )
    {
        var file = this.updateArray[i];
        
        var eventId = file.leaf.substring( 0, file.leaf.length - 4 );
        
        var event = this.iCalLib.fetchEvent( eventId );
        
        if( event )
        {
            // read the xml from the file and make an event from it.
        
            var content = this.readFile( file );
            var xmlDocument = this.makeXmlDoc( content );
        
            // convert xml document elements to event and update the event
            
            if( xmlDocument )
            {
                var eventElementList = xmlDocument.getElementsByTagName( "event" );
                
                if( eventElementList.length > 0 )
                {
                    var eventElement = eventElementList[ 0 ];
                    
                    // convert xml to ical event
                    
                    var oldString = event.getIcalString();
                    this.calendarService.fillEventFromXmlNode( event, eventElement );
                    
                    // if anything changed modify the event
                    if( event.getIcalString() != oldString )
                    {
                        CalendarEventDataSource.debug( "palm<<< sync modify: " + file.path );
                        CalendarEventDataSource.debug( "---old" + oldString );
                        CalendarEventDataSource.debug( "---new" + event.getIcalString() );
                        
                        this.iCalLib.modifyEvent( event );
                    }
                }
            }
            else
            {
                dump( "DirectorySynchronizer: Unable to read xml from file '" + file.path + "'. " ); 
            }
        }
        else
        {
            dump( "DirectorySynchronizer: Unable to sync event, event missing '" + eventId + "'. " ); 
        }
    }
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





function CalendarSyncObserver( calendarService )
{
    this.calendarService = calendarService;
}


CalendarSyncObserver.prototype.onLoad = function( calendarEvent )
{
}

CalendarSyncObserver.prototype.onAddItem = function( calendarEvent )
{
    if( calendarEvent )
    {
        if( this.calendarService.prepareSyncDirectory() )
        {
            var filePath = this.calendarService.syncPath + calendarEvent.id + ".xml";
            
            this.calendarService.writeXmlFile( filePath, [ calendarEvent ] );
            
            dump( "\n >>>palm sync add: " + filePath + "\n");
        }
    }
}

CalendarSyncObserver.prototype.onModifyItem = function( calendarEvent, originalEvent )
{
    if( !this.calendarService.gICalLib.batchMode ) // don't write the xml out for mod items when in batch mode
    {
         if( calendarEvent )
         {
            if( this.calendarService.prepareSyncDirectory() )
            {
                var filePath = this.calendarService.syncPath + calendarEvent.id + ".xml";
                
                this.calendarService.writeXmlFile( filePath, [ calendarEvent ] );
                
                CalendarEventDataSource.debug( "\n >>>palm sync modify: " + filePath + "\n");
            }
         }
    }
}

CalendarSyncObserver.prototype.onDeleteItem = function( calendarEvent )
{
    if( calendarEvent )
    {
        if( this.calendarService.prepareSyncDirectory() )
        {
            var filePath = this.calendarService.syncPath + calendarEvent.id + ".xml";
            
            CalendarEventDataSource.debug( "\n >>>palm sync delete: " + filePath + "\n");
            
            var file = new File( filePath );
            
            if( file.exists() )
            {
                file.remove();
            }
        }
    }
}

CalendarSyncObserver.prototype.onAlarm = function( calendarEvent )
{
}

CalendarSyncObserver.prototype.onStartBatch = function( )
{
}

CalendarSyncObserver.prototype.onEndBatch = function( )
{
}
