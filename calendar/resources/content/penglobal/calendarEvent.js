/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley (garths@oeone.com) , Mike Potter (mikep@oeone.com)
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
 * 
*/

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
*   Currently uses the Mcal library to store events.
*   Access to mcal is through the libxpmcal xpcom object.
*
* BUGS
*   If more than one CalendarEventDataSource is updating the 
*   mcal library, the changes will NOT be synchronized.
*   We assume that we are the ONLY one updating mcal.
*
**********
*/


/*-----------------------------------------------------------------
*  G L O B A L     V A R I A B L E S
*/

// turn on/off debuging
CalendarEvent.gDebug = false;

// Singleton CalendarEventDataSource variable.

var gCalendarEventDataSource = null;

const kCalendarXMLFileName = ".oecalendar.xml";

/*-----------------------------------------------------------------
*  C O N S T A N T S
*/


CalendarEvent.kRepeatUnit_days       = "days";
CalendarEvent.kRepeatUnit_weeks      = "weeks";
CalendarEvent.kRepeatUnit_months     = "months";
CalendarEvent.kRepeatUnit_months_day = "months_day";
CalendarEvent.kRepeatUnit_years      = "years";

CalendarEvent.kRepeatUnits = new Array( 
                                          CalendarEvent.kRepeatUnit_days, 
                                          CalendarEvent.kRepeatUnit_weeks, 
                                          CalendarEvent.kRepeatUnit_months, 
                                          CalendarEvent.kRepeatUnit_months_day, 
                                          CalendarEvent.kRepeatUnit_years 
                                      );

CalendarEvent.kAlarmUnit_minutes    = "minutes";
CalendarEvent.kAlarmUnit_hours      = "hours";
CalendarEvent.krAlarmUnit_days      = "days";
                                    
CalendarEvent.kAlarmUnits = new Array( 
                                          CalendarEvent.kAlarmUnit_minutes, 
                                          CalendarEvent.kAlarmUnit_hours, 
                                          CalendarEvent.kAlarmUnit_days 
                                      );

CalendarEvent.kAlarmEmailAddress = "";

/*-----------------------------------------------------------------
*   CalendarEventDataSource Class
*
*  Maintains all of the calendar events.
*
* PROPERTIES
*  eventTable     - array of CalendarEvent's sorted by date
*  observerList   - array of observers, see constructor notes.
*/

/**
*   CalendarEventDataSource Constructor.
* 
* PARAMETERS
*      observer     - An object that implements methods that are called when the 
*                     data source is modified. The following methods should be implemented 
*                     by the observer object ( You don't have to implement them all, just the
*                     ones you care about. )
*
                     {
                        onLoad       : function() {},                // called when server is ready
                        onAddItem    : function( calendarEvent ) {}, 
                        onModifyItem : function( calendarEvent, oldCalendarEvent ) {},
                        onDeleteItem : function( calendarEvent ) {},
                        onAlarm      : function( calendarEvent ) {},
                     };
                     
                     These methods are now called synchronously, if you add an event the onAddItem
                     method will be called during your call to add the event.

*
* NOTES
*   Is a singleton, the first invocation creates an instance, subsequent calls return the same instance.
*   Downloads all the events from the server.
*   Calls onLoad in the observer when all the data is loaded.
*/

function CalendarEventDataSource( observer, UserPath )
{
   // initialize only once
    
   if( gCalendarEventDataSource == null )
   {
      gCalendarEventDataSource = this;
        
      var mCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
            
      this.mCalLib = mCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
        
      if( UserPath == null )
         this.UserPath = penapplication.getRoot().Root.getUserPath();
      else
         this.UserPath = UserPath;

      this.mCalLib.SetServer( this.UserPath+"/.oecalendar" );
      
      // not loaded yet
      this.loaded = false;
        
      // array of events, kept sorted by date
      this.eventTable = new Array();
        
      // set up the observer list
      this.observerList = new Array();
        
      // add the observer BEFORE loading events
        
      if( observer )
      {
         this.addObserver(  observer );
      }
        
      this.timers = new Array();

      // load ALL the events from the server
       
      this.loadEventsAsync( );
   }
   else
   {
      gCalendarEventDataSource.addObserver(  observer );
   }
    
   // return the singleton
      
   return gCalendarEventDataSource;
}



/** PUBLIC
*
*   CalendarEventDataSource/addObserver.
*
*      Add an observer, makes sure the same observer is not added twice.
*
* PARAMETERS
*      observer     - Observer to add.
*/

CalendarEventDataSource.prototype.addObserver = function( observer )
{
   if( observer )
   {
      // remove it first, that way no observer gets added twice
      
      this.removeObserver(  observer );
      
      // add it to the end.
      
      if( CalendarEvent.gDebug )
         debug( "\n-------CalendarEventDataSource---adding observer------------\n" );
      
      this.observerList[ this.observerList.length ] = observer;
      
      // if we have already loaded, call the observer's on load
      
      if( this.loaded  )
      {
         if( observer.onLoad )
         {
            observer.onLoad()
         }
      }
   }
}



/** PUBLIC
*
*   CalendarEventDataSource/removeObserver.
*
*      Removes a previously added observer 
*
* PARAMETERS
*      observer     - Observer to remove.
*/

CalendarEventDataSource.prototype.removeObserver = function( observer )
{
   for( var index = 0; index < this.observerList.length; ++index )
   {
      if( observer === this.observerList[ index ] )
      {
         if( CalendarEvent.gDebug )
            debug( "\n-------CalendarEventDataSource---removing observer------------\n" );
         
         this.observerList.splice( index, 1 );
         return true;
      }
   }
   
   return false;
}


/** PUBLIC
*
*   CalendarEventDataSource/addEvent.
*
*      Add a new event to the data source. 
*
* PARAMETERS
*      calendarEvent     - new event object to add, This object itself will be added
*                          to the list, some of its data may be modified in place
*
* NOTES
*      All observers will be notified when the add actually happens
*/


CalendarEventDataSource.prototype.addEvent = function( calendarEvent )
{
    // convert to mcal
    
    var mCalLibEvent = calendarEvent.convertToMcalEvent();
    
    // do the add
    
	 var id = this.mCalLib.AddEvent( mCalLibEvent );

    var newMcalLibEvent = this.mCalLib.FetchEvent( id );
    
    //convert & insert
    CalendarEvent.makeFromMcalEvent( newMcalLibEvent, calendarEvent );

    // update based on new event
    
    var eventTable = this.getTable();
   
    eventTable[ eventTable.length ] = calendarEvent;
   
    // keep it sorted by date
    eventTable.sort( CalendarEvent.orderEventsByDate );
         
    this.setUpAlarmTimers();
   
    // call observer
   
    for( var index in this.observerList )
    {
      var observer = this.observerList[ index ];
      
      if( observer.onAddItem )
      {
         observer.onAddItem( calendarEvent );
      }
    }

    //this.writeXmlFile( this.UserPath+"/"+kCalendarXMLFileName );
}



/** PUBLIC
*
*   CalendarEventDataSource/modifyEvent.
*      Modify an event
*
* PARAMETERS
*      calendarEvent     - The event object to modify. This MUST be an object that
*                          you got from the datasource.
*                          The object will be modified IN PLACE
*                          
*                          The object will be passed into your observer's onModifyItem method.
*              
*/


CalendarEventDataSource.prototype.modifyEvent = function( calendarEvent )
{
    // so we can pass the original and modified event into the observers
	
    var oldMCalLibEvent = this.mCalLib.FetchEvent( calendarEvent.id );
    
    var originalEvent = calendarEvent.clone();
    
    CalendarEvent.makeFromMcalEvent( oldMCalLibEvent, originalEvent );
      
    // convert to mcal event
    
    var mCalLibEvent = calendarEvent.convertToMcalEvent();
    
    //set the modified time to now; this apparently does not work.
    //mCalLibEvent.lastModified = new Date();

    this.mCalLib.UpdateEvent( mCalLibEvent );
    
    // keep it sorted in case the date changes
         
    var eventTable = this.getTable();
         
    eventTable.sort( CalendarEvent.orderEventsByDate );
         
    this.setUpAlarmTimers();
   
    // call observers
         
    for( var index in this.observerList )
    {
      var observer = this.observerList[ index ];
           
      if( observer.onModifyItem )
      {
         observer.onModifyItem( calendarEvent, originalEvent );
      }
    } 

    //this.writeXmlFile( this.UserPath+"/"+kCalendarXMLFileName );
}


/** PUBLIC
*
*   CalendarEventDataSource/deleteEvent.
*
* PARAMETERS
*      calendarEvent     - The event object to delete. This MUST be an object that
*                          you got from the datasource.
*                          
*                          Your observer's onDeletItem method will be called.
*/

CalendarEventDataSource.prototype.deleteEvent = function( calendarEvent )
{
    // do delete
    
	this.mCalLib.DeleteEvent( calendarEvent.id );
    
   var nextEvent = this.getNextEventFromTable( calendarEvent );
      
   // get it out of the table
   
   this.removeEventFromTable( calendarEvent );
   
   this.setUpAlarmTimers();
   
   // notify observer
   
   for( var index in this.observerList )
   {
      var observer = this.observerList[ index ];
      
      if( observer.onDeleteItem )
      {
         observer.onDeleteItem( calendarEvent, nextEvent );
      }
   }
   
   //this.writeXmlFile( this.UserPath+"/"+kCalendarXMLFileName );
}
    


/** PUBLIC
*
*   CalendarEventDataSource/getCalendarEventById.
*
*      Lookup an event by id. 
*/

CalendarEventDataSource.prototype.getCalendarEventById = function( id )
{
   var eventTable = this.getTable();
   
   for( var index = 0; index < eventTable.length; ++index )
   {
      var calendarEvent = eventTable[ index ];
      
      if( calendarEvent.id == id )
         return calendarEvent;
   }
   
   return null;
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
      var eventTable = this.getTable();
      
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
               }
            }
         }
         else if ( typeof fieldName == "object" ) 
         {
            for ( i in fieldName ) 
            {
               var value = calendarEvent[ fieldName[i] ].toLowerCase();
         
               if( value )
               {
                  if( value.indexOf( searchText ) != -1 )
                  {
                     searchEventTable[ searchEventTable.length ]  =  calendarEvent; 
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
   var monthToFind = date.getMonth();
   var yearToFind = date.getFullYear();
   
   // make a new array for the month's events
   
   var monthEvents =  new Array();
   
   // run thru the table looking for matching start dates
   
   var eventTable = this.getTable();
   
   for( var eventIndex = 0; eventIndex < eventTable.length; ++eventIndex )
   {
      var calendarEvent = eventTable[ eventIndex ];
      
      var eventDate = calendarEvent.start;
      
      if( eventDate.getFullYear() == yearToFind  && eventDate.getMonth() == monthToFind  )
      {
         // A MATCH, add it to the array
         
         monthEvents[ monthEvents.length ] = calendarEvent;
      }

      //check to see if this is a repeating event.
      if ( calendarEvent.recurType != 0 ) 
      {
         if ( eventDate.getMonth() == monthToFind && eventDate.getFullYear() == yearToFind ) 
         {
            var CheckDateInMs = eventDate.getTime();
            
            CheckDateInMs = CheckDateInMs + (60 * 60 * 24 * 1000 );

            var newDate = new Date( CheckDateInMs );

            var DateToCheck = newDate.getDate();
            var YearToCheck = newDate.getFullYear();
            var MonthToCheck = newDate.getMonth();
         }
         else
         {
            var CheckDateInMs = new Date( yearToFind, monthToFind, 1, 0, 0, 0 );

            CheckDateInMs = CheckDateInMs - ( 1000 * 60 * 60 * 24 );

            var newDate = new Date( CheckDateInMs );
            
            var DateToCheck = newDate.getDate();
            var YearToCheck = newDate.getFullYear();
            var MonthToCheck = newDate.getMonth();
         }
         
         var mCalLibEvent = this.mCalLib.FetchEvent( calendarEvent.id );
         
         //get all the occurences of this event in this month.
         
         var result = true;
         
         while( result )
         {
            dump( "\n-->about to check for "+YearToCheck+" and MonthToCheck "+MonthToCheck +" and DateToCheck"+DateToCheck+"\n" );
            result = mCalLibEvent.GetNextRecurrence( YearToCheck, MonthToCheck, DateToCheck, 0, 0 );

            if ( !result ) 
            {
               break;
            }

            var ResultArray = result.split( "," );
            
            if( ResultArray[0] == YearToCheck && ResultArray[1] == MonthToCheck && ResultArray[2] == DateToCheck )
            {
               break;
            }

            YearToCheck = ResultArray[0];
            MonthToCheck = ResultArray[1];
            DateToCheck = ResultArray[2];
            dump( "\n-->After GetNextRecurrence, YearToCheck = "+YearToCheck+" MonthToCheck = "+MonthToCheck+" DateToCheck is "+DateToCheck );

            if ( YearToCheck == yearToFind && MonthToCheck == monthToFind ) 
            {
               var newCalendarEvent = calendarEvent.clone( true );
               //change its display day to the one for repeating events.
               newCalendarEvent.displayDate = new Date( YearToCheck, MonthToCheck, DateToCheck, calendarEvent.start.getHours(), calendarEvent.start.getMinutes(), 0 );
               
               monthEvents[ monthEvents.length ] = newCalendarEvent;
            } 
            else if ( YearToCheck != yearToFind || MonthToCheck != monthToFind ) 
            {
               break;
            }

            //add one day less 1 hour to avoid getting the same event again.
            var CheckDate = new Date( YearToCheck, MonthToCheck, DateToCheck, calendarEvent.start.getHours(), calendarEvent.start.getMinutes(), 0 );
            var CheckDateInMs = CheckDate.getTime();

            CheckDateInMs = CheckDateInMs + ( 1000 * 60 * 60 * 24 );
            var NewDate = new Date( CheckDateInMs );
            YearToCheck = NewDate.getFullYear();
            MonthToCheck = NewDate.getMonth();
            DateToCheck = NewDate.getDate();
         }
      }
   }
   
   // return the month's events
   
   return monthEvents
   
}



/** PUBLIC
*
*   CalendarEventDataSource/debugEvents.
*
*/

CalendarEventDataSource.prototype.debugEvents = function(  )
{
   var eventTable = this.getTable();
   
   debug( "\n==================================================================== " + eventTable.length );
   
   for( var eventIndex = 0; eventIndex < eventTable.length; ++eventIndex )
   {
      var calendarEvent = eventTable[ eventIndex ];
      
      var eventDate = calendarEvent.start;
      
      debug( "\n-------- " + calendarEvent.id + " " + eventDate.getFullYear() + " " + eventDate.getMonth() );
      
   }
   
   debug( "\n==================================================================== " );
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
   
   var eventTable = this.getTable();
   
   return eventTable.slice( 0 );
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
   // clone the array in case the caller messes with it
   
   var eventTable = this.getTable();
      
   var Today = new Date();

   for( i = 0; i < eventTable.length; i++ )
   {
      if( eventTable[i].start > Today )
         break;
   }
   
   return eventTable.slice( i, ( i + parseInt( EventsToGet ) ) );
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
   // match on the month
   monthEventList = this.getTable();
   
   var monthToFind = date.getMonth();
   var yearToFind = date.getFullYear();
   var dateToFind = date.getDate();

   // make array for day events
   
   var dayEvents =  new Array();
   
   // search month list for matching date
   
   var dateToFind = date.getDate();
   
   for( var eventIndex = 0; eventIndex < monthEventList.length; ++eventIndex )
   {
      var calendarEvent = monthEventList[ eventIndex ];
      
      var eventDate = calendarEvent.displayDate;
      
      //if( eventDate.getDate() == dateToFind )
      if( eventDate.getMonth() == date.getMonth() && eventDate.getFullYear() == date.getFullYear() && eventDate.getDate() == date.getDate() )
      {
         // MATCH: add it to the list
         
         dayEvents[ dayEvents.length ] = calendarEvent;
      }
      
      //check to see if this is a repeating event.
      if ( calendarEvent.recurType != 0 ) 
      {
         //if it is to be shown today.
         //if ( eventDate.getMonth() == monthToFind && eventDate.getFullYear() == yearToFind && eventDate.getDate() == dateToFind ) 
         //{
            var EventDateInMs = eventDate.getTime();
            
            EventDateInMs = EventDateInMs + (60 * 60 * 23 * 1000 );

            var newDate = new Date( EventDateInMs );

            var DateToCheck = newDate.getDate();

            var YearToCheck = newDate.getFullYear();

            var MonthToCheck = newDate.getMonth();
         /*}
         else
         {
            var DateToCheck = 1;
            var YearToCheck = yearToFind;
            var MonthToCheck = monthToFind;
         } */
         
         var mCalLibEvent = this.mCalLib.FetchEvent( calendarEvent.id );
         
         //get all the occurences of this event in this month.
         
         var result = true;
         
         while( result )
         {
            dump( "\n-->about to check for "+YearToCheck+" and MonthToCheck "+MonthToCheck +" and DateToCheck"+DateToCheck+"\n" );
            result = mCalLibEvent.GetNextRecurrence( YearToCheck, MonthToCheck, DateToCheck, 0, 0 );
            
            if ( !result ) 
            {
               break;
            }

            var ResultArray = result.split( "," );
            
            YearToCheck = ResultArray[0];
            MonthToCheck = ResultArray[1];
            DateToCheck = ResultArray[2];
            
            var NextEventDate = new Date( YearToCheck, MonthToCheck, DateToCheck, 0, 0, 0 );

            if ( YearToCheck == yearToFind && MonthToCheck == monthToFind && DateToCheck == dateToFind ) 
            {
               var newCalendarEvent = calendarEvent.clone( true );
               
               //change its display day to the one for repeating events.
               newCalendarEvent.displayDate = new Date( YearToCheck, MonthToCheck, DateToCheck, calendarEvent.start.getHours(), calendarEvent.start.getMinutes(), 0 );
               
               dayEvents[ dayEvents.length ] = newCalendarEvent;
            } 
            else if ( NextEventDate > date ) 
            {
               break;
            }

            //add one day less 1 hour to avoid getting the same event again.
            var CheckDate = new Date( YearToCheck, MonthToCheck, DateToCheck, calendarEvent.start.getHours(), calendarEvent.start.getMinutes(), 0 );
            var CheckDateInMs = CheckDate.getTime();

            CheckDateInMs = CheckDateInMs + ( 1000 * 60 * 60 * 23 );
            var NewDate = new Date( CheckDateInMs );
            YearToCheck = NewDate.getFullYear();
            MonthToCheck = NewDate.getMonth();
            DateToCheck = NewDate.getDate();
         }
      }
   }
   
   // return day's events
   
   return dayEvents;
   
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
   // match on the month
   weekEventList = this.getTable();
   
   var monthToFind = date.getMonth();
   var yearToFind = date.getFullYear();
   var dateToFind = date.getDate();

   // make array for day events
   
   var weekEvents =  new Array();
   
   var StartOfWeek = new Date( date.getFullYear(), date.getMonth(), date.getDate(), 0, 0, 0 );
   
   var EndOfWeek = new Date( date.getFullYear(), date.getMonth(), date.getDate() + 7, 0, 0, 0 );
   
   // search month list for matching date
   
   var dateToFind = date.getDate();
   
   for( var eventIndex = 0; eventIndex < weekEventList.length; ++eventIndex )
   {
      var calendarEvent = weekEventList[ eventIndex ];
      
      var eventDate = calendarEvent.displayDate;
            
      //if the event happens in this week.
      if( eventDate.getTime() >= StartOfWeek.getTime() && eventDate.getTime() <= EndOfWeek.getTime() )
      {
         // MATCH: add it to the list
         
         weekEvents[ weekEvents.length ] = calendarEvent;
      }
      
      //check to see if this is a repeating event.
      if ( calendarEvent.recurType != 0 ) 
      {
         //if it happens in this week.
         if ( eventDate.getTime() >= StartOfWeek.getTime() && eventDate.getTime() <= EndOfWeek.getTime() ) 
         {
            var CheckDateInMs = eventDate.getTime();
            
            CheckDateInMs = CheckDateInMs + (60 * 60 * 23 * 1000 );

            var newDate = new Date( CheckDateInMs );

            var DateToCheck = newDate.getDate();

            var YearToCheck = newDate.getFullYear();

            var MonthToCheck = newDate.getMonth();
         }
         else
         {
            var DateToCheck = 1;
            var YearToCheck = yearToFind;
            var MonthToCheck = monthToFind;
         }
         
         var mCalLibEvent = this.mCalLib.FetchEvent( calendarEvent.id );
         
         //get all the occurences of this event in this month.
         
         var result = true;
         
         while( result )
         {
            result = mCalLibEvent.GetNextRecurrence( YearToCheck, MonthToCheck, DateToCheck, 0, 0 );
            
            if ( !result ) 
            {
               break;
            }

            var ResultArray = result.split( "," );
            
            YearToCheck = ResultArray[0];
            MonthToCheck = ResultArray[1];
            DateToCheck = ResultArray[2];

            var NextEventDate = new Date( YearToCheck, MonthToCheck, DateToCheck, 0, 0, 0 );
            
            if ( NextEventDate.getTime() >= StartOfWeek.getTime() && NextEventDate.getTime() <= EndOfWeek.getTime() ) 
            {
               var newCalendarEvent = calendarEvent.clone( true );
               
               //change its display day to the one for repeating events.
               newCalendarEvent.displayDate = new Date( YearToCheck, MonthToCheck, DateToCheck, calendarEvent.start.getHours(), calendarEvent.start.getMinutes(), 0 );
               
               weekEvents[ weekEvents.length ] = newCalendarEvent;
            } 
            else if ( NextEventDate.getTime() > EndOfWeek.getTime() ) 
            {
               break;
            }

            //add one day less 1 hour to avoid getting the same event again.
            var CheckDate = new Date( YearToCheck, MonthToCheck, DateToCheck, calendarEvent.start.getHours(), calendarEvent.start.getMinutes(), 0 );
            var CheckDateInMs = CheckDate.getTime();

            CheckDateInMs = CheckDateInMs + ( 1000 * 60 * 60 * 23 );
            var NewDate = new Date( CheckDateInMs );
            YearToCheck = NewDate.getFullYear();
            MonthToCheck = NewDate.getMonth();
            DateToCheck = NewDate.getDate();
         }
      }
   }
   
   // return day's events
   
   return weekEvents;
   
}


/** PUBLIC
* 
* RETURN
*    An xml document with all the event info
*/

CalendarEventDataSource.prototype.makeXmlDocument = function( )
{
    // use the domparser to create the XML 
    var domParser = Components.classes["@mozilla.org/xmlextras/domparser;1"].getService( Components.interfaces.nsIDOMParser );
    
    // start with one tag
    var xmlDocument = domParser.parseFromString( "<events/>", "text/xml" );
    
    // get the top tag, there will only be one.
    var topNodeList = xmlDocument.getElementsByTagName( "events" );
    var topNode = topNodeList[0];
    
    // add each event as an element
    for( var index = 0; index < this.eventTable.length; ++index )
    {
        var calendarEvent = this.eventTable[ index ];
        
        var eventNode = calendarEvent.makeXmlNode( xmlDocument );
        
        topNode.appendChild( eventNode );
    }

    // return the document
    
    return xmlDocument;
}

/** PUBLIC
*    write an xml document with all the event info
*/

CalendarEventDataSource.prototype.writeXmlFile = function( path )
{
    var xmlDoc = this.makeXmlDocument();
    
    var domSerializer = Components.classes["@mozilla.org/xmlextras/xmlserializer;1"].getService( Components.interfaces.nsIDOMSerializer );
    var content = domSerializer.serializeToString( xmlDoc );
    
    penFileUtils.writeFile( path, content );
}


/** PRIVATE
*
*   CalendarEventDataSource/getTable.
*
*      return table of events. 
*/

CalendarEventDataSource.prototype.getTable = function ()
{
   return this.eventTable;
}



/** PRIVATE
*
*   CalendarEventDataSource/loadEventsAsync.
*
*      Load events from Mcal on the server, called when first constructed. 
*/

CalendarEventDataSource.prototype.loadEventsAsync = function( )
{
    // call to load the ids
    
    var start = new Date( 0, 0, 1, 0, 0, 0 );
    var end = new Date( 9999, 11, 31, 23, 59, 59 );
	
    var idliststr = this.mCalLib.SearchEvent( 
                                     start.getFullYear(),
									 start.getMonth()+1,
									 start.getDate(),
									 start.getHours(),
									 start.getMinutes(),
									 end.getFullYear(),
									 end.getMonth()+1,
									 end.getDate(),
									 end.getHours(),
									 end.getMinutes() );
    
    var mCalEventArray = new Array();
    
    if( idliststr )
    {
    	var idarray = idliststr.split( "," );
        
        // get the ids as mcal events
        
        for( index in idarray )
        {
            if( idarray[ index ] != "" )
            {
                var tmpevent = this.mCalLib.FetchEvent( idarray[ index ] )
                mCalEventArray[ mCalEventArray.length ] =  tmpevent;
            }
        }
    }
    
    // convert all the events from mcal to our own instances
   
   this.eventTable = CalendarEventDataSource.convertEventArray( mCalEventArray );
   
   // sort them by date, we keep them that way
   
   this.eventTable.sort( CalendarEvent.orderEventsByDate );
   
   // set up event alarm timers
   
   this.setUpAlarmTimers();
   
   // debugging 
   
   if( CalendarEvent.gDebug )
      debug( "\n-------OnLoad---start----\n" + this.eventTable.toSource() +  "\n---------OnLoad-end--------\n" );
   
   // Loading is done, notify observers 
   
   this.loaded = true;
   
   for( var index in this.observerList )
   {
      var observer = this.observerList[ index ];
      
      if( observer.onLoad )
      {
         observer.onLoad();
      }
   }
}


/** PRIVATE STATIC
*
*   CalendarEventDataSource/convertEventArray.
*
*      Convert an array of mcal events to our events 
*/

CalendarEventDataSource.convertEventArray = function( eventArray )
{
   var newArray = new Array();
   
   for( index in eventArray )
   {
      var calendarEvent = new CalendarEvent();
      
      CalendarEvent.makeFromMcalEvent( eventArray[ index ], calendarEvent );
      
      newArray[ newArray.length ] =  calendarEvent;  
   }
   
   return newArray;
}
  

/** PRIVATE
*
*   CalendarEventDataSource/setUpAlarmTimers.
*
*      create timers for each event that has an alarm that hasn't gone off.
*/

CalendarEventDataSource.prototype.setUpAlarmTimers = function( )
{
   for( i = 0; i < this.timers.length; i++ )
   {
      window.clearTimeout( this.timers[i] );
   }

   var Today = new Date( );

   var idliststr = this.mCalLib.SearchAlarm( Today.getFullYear(), ( Today.getMonth() + 1 ), Today.getDate(), Today.getHours(), Today.getMinutes() );

   var mCalEventArray = new Array();
    
   if( idliststr )
   {
      var idarray = idliststr.split( "," );
        
      // get the ids as mcal events
        
      for( index in idarray )
      {
         if( idarray[ index ] != "" )
         {
            var tmpevent = this.mCalLib.FetchEvent( idarray[ index ] )
                
            mCalEventArray[ mCalEventArray.length ] =  tmpevent;
         }
      }
   }

   this.alarmTable = Array();

   // convert all the events from mcal to our own instances
   
   this.alarmTable = CalendarEventDataSource.convertEventArray( mCalEventArray );
   
   // sort them by date, we keep them that way
   
   this.alarmTable.sort( CalendarEvent.orderEventsByDate );

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

/** PRIVATE
*
*   CalendarEventDataSource/respondAlarmEvent.
*
*      Response method for an alarm going off
*/


CalendarEventDataSource.respondAlarmTimeout = function( calendarEventID )
//function respondAlarmTimeout( calendarEventID )
{
   var calendarEvent = gCalendarEventDataSource.getCalendarEventById( calendarEventID );
   
   //if( calendarEvent && calendarEvent.checkAlarm() )
   //{
      debug( "in CalendarEventDataSource.prototype.respondAlarmEvent\n+++++++++ALARM WENT OFF \n" + 
                     calendarEvent.toSource() + 
                     "\n at " + 
                     calendarEvent.start 
                     + "\n+++++++++\n" );
      
      // :TODO: The email sending stuff should be part of the CalendarEvent class
      
      if ( calendarEvent.alarmEmailAddress )
      {
         
         //debug( "about to send an email to "+ calendarEvent.alarmEmailAddress + "with subject "+ calendarEvent.title );
         
         //calendar.sendEmail( "Calendar Event", 
        //                     "Title: "+ calendarEvent.title + " @ "+ calendarEvent.start + "\nThis message sent from OECalendar.", 
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



/** PRIVATE
*
*   CalendarEventDataSource/removeEventFromTable.
*
*      Remove an event object from the table, if it is there
*/


CalendarEventDataSource.prototype.removeEventFromTable = function( calendarEvent )
{
   var eventTable = this.getTable();
   
   for( var index = 0; index < eventTable.length; ++index )
   {
      if( calendarEvent === eventTable[ index ] )
      {
         eventTable.splice( index, 1 );
         break;
      }
   }
}



/** PRIVATE
*
*   CalendarEventDataSource/getNextEventFromTable
*
*      Get the index and the event after the passed in one. 
*      If none, gets the previous event.
*      If no previous event, return false.
*/
CalendarEventDataSource.prototype.getNextEventFromTable = function( calendarEvent )
{
   var eventTable = this.getTable();
   
   for( var index = 0; index < eventTable.length; ++index )
   {
      if( calendarEvent === eventTable[ index ] )
      {
         if ( eventTable[ ++index ]) 
         {
            return( eventTable[ index ] );
         }
         else if ( eventTable[ --index ]) 
         {
            return( eventTable[ --index ] );
         }
         else
            return false;
      }
   }
   if ( eventTable[ --index ]) 
   {
      return( eventTable[ --index ] );
   }
   else
      return false;
}




/*-----------------------------------------------------------------
*   CalendarEvent Class
*
*  Maintains info about a calendar event.
*
* PROPERTIES
*/

/**
*   CalendarEvent Constructor.
* 
* NOTES
*   Use static factory methods makeNewEvent and makeFromMcalEvent to create event objects
*/

function CalendarEvent()
{

}



/** PUBLIC STATIC
*   CalendarEvent Factory.
* 
* NOTES
*   make a new instance of CalendarEvent with all the basics filled in
*/


CalendarEvent.makeNewEvent = function( startDate, allDay )
{
   var calendarEvent = new CalendarEvent();
   
   // copy the date since it may be changed in place
   
   calendarEvent.id = null;
   calendarEvent.start = new Date( startDate );
   
   calendarEvent.end = new Date( calendarEvent.start );
   calendarEvent.end.setHours( calendarEvent.end.getHours() + 1 );
   
   calendarEvent.allDay = allDay;
   
   calendarEvent.title       = "";
   calendarEvent.category    = "";
   calendarEvent.description = "";
   calendarEvent.location    = "";
   
   calendarEvent.lastModified = new Date();

   calendarEvent.privateEvent = true;
   calendarEvent.inviteEmailAddress = "";
   
   calendarEvent.alarm       = false;
   calendarEvent.alarmLength = 15;
   calendarEvent.alarmUnits  = CalendarEvent.kAlarmUnits_minutes;  
   calendarEvent.alarmWentOff = false;
   calendarEvent.alarmEmailAddress  = CalendarEvent.kAlarmEmailAddress;
   calendarEvent.snoozeTime;
   
   calendarEvent.repeat         = false;
   calendarEvent.recurInterval  = 1;
   calendarEvent.repeatUnits    = CalendarEvent.kRepeatUnit_weeks; 
   calendarEvent.repeatForever  = true;
   calendarEvent.recurEnd       = new Date();
   calendarEvent.recurType      = 0;
   
   return calendarEvent;
}


/** PUBLIC
* 
* NOTES
*   make a new instance of CalendarEvent with the same values as this. Date values
*   are copied since dates can be changed in place.
*/


CalendarEvent.prototype.clone = function( KeepId )
{
   var calendarEvent = new CalendarEvent();
   
   // copy the date since it may be changed in place
   
   if ( KeepId ) 
   {
      calendarEvent.id             = this.id;
   }
   else
   {
      calendarEvent.id             = null;
   }
   
   
   calendarEvent.start          = new Date( this.start );
   calendarEvent.displayDate    = new Date( this.displayDate );
   calendarEvent.end            = new Date( this.end );
   calendarEvent.allDay         = this.allDay;
   
   calendarEvent.title          = this.title;
   calendarEvent.description    = this.description;
   calendarEvent.category       = this.category;
   calendarEvent.location       = this.location;
   calendarEvent.privateEvent   = this.privateEvent;
   calendarEvent.lastModified   = this.lastModified;
   
   calendarEvent.alarm              = this.alarm;
   calendarEvent.alarmLength        = this.alarmLength;
   calendarEvent.alarmUnits         = this.alarmUnits;  
   calendarEvent.alarmWentOff       = this.alarmWentOff;
   calendarEvent.alarmEmailAddress  = this.alarmEmailAddress;
   calendarEvent.snoozeTime         = this.snoozeTime;
   
   calendarEvent.repeat         = this.repeat;
   calendarEvent.repeatUnits    = this.repeatUnits; 
   calendarEvent.repeatForever  = this.repeatForever;
   calendarEvent.recurInterval  = this.recurInterval;
   calendarEvent.recurEnd       = new Date( this.recurEnd );
   calendarEvent.recurType      = this.recurType;
   
   return calendarEvent;
}


/** PUBLIC
* 
* PARAMETERS
*    xmlDocument - the document is to be used to create the node
*
* RETURN
*    An xml node with all the event info
*/

CalendarEvent.prototype.makeXmlNode = function( xmlDocument )
{
    var checkDate = function( date )
    {
        if( date && date.constructor === Date )
            return date.getTime();
        else
            return ""
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

    eventNode.setAttribute( "id",               this.id );
    
    eventNode.setAttribute( "start",            checkDate( this.start ) );
    eventNode.setAttribute( "displayDate",      checkDate( this.displayDate ) );
    eventNode.setAttribute( "end",              checkDate( this.end ) );
    eventNode.setAttribute( "allDay",           checkBoolean( this.allDay ) );
    
    
    
    eventNode.setAttribute( "title",            checkString( this.title ) );
    eventNode.setAttribute( "description",      checkString( this.description ) );
    eventNode.setAttribute( "category",         checkString( this.category ) );
    eventNode.setAttribute( "location",         checkString( this.location ) );
    eventNode.setAttribute( "privateEvent",     checkBoolean( this.privateEvent ) );
    eventNode.setAttribute( "lastModified",     checkDate( this.lastModified ) );
    
    eventNode.setAttribute( "alarm",            checkDate( this.alarm ) );
    eventNode.setAttribute( "alarmLength",      checkNumber( this.alarmLength ) );
    eventNode.setAttribute( "alarmUnits",       checkString( this.alarmUnits ) );  
    eventNode.setAttribute( "alarmWentOff",     checkBoolean( this.alarmWentOff ) );
    eventNode.setAttribute( "alarmEmailAddress",checkString( this.alarmEmailAddress ) );
    eventNode.setAttribute( "snoozeTime",       checkNumber( this.snoozeTime ) );
    
    eventNode.setAttribute( "repeat",           checkBoolean( this.repeat ) );
    eventNode.setAttribute( "repeatUnits",      checkString( this.repeatUnits ) ); 
    eventNode.setAttribute( "repeatForever",    checkBoolean( this.repeatForever ) );
    eventNode.setAttribute( "recurInterval",    checkNumber( this.recurInterval ) );
    eventNode.setAttribute( "recurEnd",         checkDate( this.recurEnd ) );
    eventNode.setAttribute( "recurType",        checkNumber( this.recurType ) );
   
    return eventNode;
}



/** PUBLIC 
* 
* PARAMETERS
*
* RETURN  null - if the event is OK.
*         object  - if there are problems, the object has two properties, missing and invalid, 
*                   each contains an array.
*                   
*              object.missing - array of names of missing required properties     
*              object.invalid - array of objects, .name - name of field
*                                                 .reason - description of what is wrong.   
*
* NOTES
*   Make sure event has legal values
*/
   
CalendarEvent.prototype.verifyEvent = function( )
{
   // check for all required values and return a list of missing ones
   
   var missing = new Array();
   
   if( (!this.title) || typeof this.title != "string" || this.title.length == 0 )
   {
      missing[ missing.length ] = "title"
   }
  
   if( (!this.start) || typeof this.start  != "object" ||  this.start.constructor  != Date )
   {
      missing[ missing.length ] = "start"
   }
   
   // check for invalid values
   
   var invalid = new Array();
   
   // check alarm units
   
   var alarmUnitsOK = false;
   
   for( var i = 0; i < CalendarEvent.kAlarmUnits.length; ++i )
   {
      if( CalendarEvent.kAlarmUnits[ i ] == this.alarmUnits )
      {
         alarmUnitsOK = true;
         break;
      }
   }
   
   if( !alarmUnitsOK ) 
   {
      invalid[ invalid.length ] = {name:"alarmUnits", reason:"Alarm units must be one of " + CalendarEvent.kAlarmUnits.join() };
   }
   
   
   // check repeat units
   
   var repeatUnitsOK = false;
   
   for( var i = 0; i < CalendarEvent.kRepeatUnits.length; ++i )
   {
      if( CalendarEvent.kRepeatUnits[ i ] == this.repeatUnits )
      {
         repeatUnitsOK = true;
         break;
      }
   }
   
   if( !repeatUnitsOK ) 
   {
      invalid[ invalid.length ] = {name:"repeatUnits", reason:"Repeat units must be one of " + CalendarEvent.kRepeatUnits.join() };
   }
   
   
   // return NULL if it is OK, an error list if it is NOT
   
   if( invalid.length > 0 || missing.length > 0 )
   {
      return { missing: missing, invalid: invalid };
   }
   else
   {
      return null;
   }
}


/** PUBLIC
* 
* PARAMETERS
*        minBefore - number of minutes BEFORE the event that the alarm is to be set for.
*        update    - if true the datasource is updated right away.
*/


CalendarEvent.prototype.setAlarmMinutes = function( minsBefore, update )
{
   return this.setAlarm( CalendarEvent.kAlarmUnit_minutes, minsBefore, update );
}


CalendarEvent.prototype.setAlarmHours = function( hoursBefore, update )
{
   return this.setAlarm( CalendarEvent.kAlarmUnit_hours, hoursBefore, update );
}


CalendarEvent.prototype.setAlarmDays = function( daysBefore, update )
{
   return this.setAlarm( CalendarEvent.kAlarmUnit_days, daysBefore, update );
}


/** PRIVATE
* 
* NOTES
*        Helper function for setting alarms
* PARAMETERS
*        minBefore - number of minutes BEFORE the event that the alarm is to be set for.
*        update    - if true the datasource is updated right away.
*/


CalendarEvent.prototype.setAlarm = function( units, timeBefore, update )
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
}   


/** PUBLIC
* 
* PARAMETERS
*
* RETURN 
*         true if the alarm for this event should be sounded
*/


CalendarEvent.prototype.checkAlarm = function( )
{
   if( this.alarm && ( !this.alarmWentOff || ( this.alarmWentOff && this.snoozeTime ) ) )
   {
      var alarmTimeMS = this.calculateAlarmTime(); 
      
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
}


/** PRIVATE
* 
* PARAMETERS
*
* RETURN 
*       calculate the date in milliseconds at which an alarm should go off, return null if no alarm set
*/


CalendarEvent.prototype.calculateAlarmTime = function( )
{
   //if( this.alarm )
   //{
      var timeInAdvance =  this.alarmLength;
      
      var timeInAdvanceUnits =  this.alarmUnits;
      
      var timeInAdvanceInMS;
      
      debug( "\nIn calculate alarm Time, alarmUnits is "+ this.alarmUnits+"\n" );
      
      switch( this.alarmUnits )
      {  
         case CalendarEvent.kAlarmUnit_minutes:   timeInAdvanceInMS = timeInAdvance * kDate_MillisecondsInMinute;  break;
         case CalendarEvent.kAlarmUnit_hours:     timeInAdvanceInMS = timeInAdvance * kDate_MillisecondsInHour;    break;
         case CalendarEvent.kAlarmUnit_days:      timeInAdvanceInMS = timeInAdvance * kDate_MillisecondsInDay;     break;
         default:                                 timeInAdvanceInMS = timeInAdvance * kDate_MillisecondsInMinute;  break;
      }
      
      var eventTimeInMS = this.start.getTime();
      
      var alarmTimeInMS = eventTimeInMS - timeInAdvanceInMS;
      
      return  alarmTimeInMS;
    //}
    //else
    //{
    //  return false;
    //}
}


/** PRIVATE
* 
* PARAMETERS
*
* RETURN 
*         clear the alarm timer if there is one
*/


CalendarEvent.prototype.clearAlarmTimer = function( )
{
   if( this.timerID )
   {
      debug( "clearing alarm for :" + this.id );
      
      window.clearTimeout( this.timerID  );
      this.timerID = null;
   }
}

/** PRIVATE
* 
* PARAMETERS
*
* RETURN 
*         true if the alarm for this event should be sounded
*/


CalendarEvent.prototype.setUpAlarmTimer = function( dataSource )
{
   // remove any existing timer
   
   this.clearAlarmTimer();
  
   // set up the new timer
   debug( "this.alarm is "+this.alarm );

   if( this.alarm  && ( !this.alarmWentOff || ( this.alarmWentOff && this.snoozeTime ) ) )
   {
      debug( "testing alarm for :" + this.title + "  " + this.start );
      
      var alarmTimeMS = this.calculateAlarmTime(); 
      
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
         
         debug( "!-- SNOOZE IS TRUE! nowMS < this.start in calendarEvent.js, timeTillAlarm is "+ timeTillAlarm +" \n" );

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
         
         debug( "!-- NO SNOOZE! nowMS < this.start in calendarEvent.js, timeTillAlarm is "+ timeTillAlarm +" \n" );

         if( timeTillAlarm < 10 )
         {
            timeTillAlarm = 10;
         }
          
         debug( "\n Setting alarm for MS:" + timeTillAlarm );
         
         this.timerID = window.setTimeout( "CalendarEventDataSource.respondAlarmTimeout(" + this.id + ")", timeTillAlarm  );

      }
   }
}



/** PACKAGE STATIC
*   CalendarEvent orderEventsByDate.
* 
* NOTES
*   Used to sort table by date
*/

CalendarEvent.orderEventsByDate = function( eventA, eventB )
{
   return( eventA.start.getTime() - eventB.start.getTime() );
}

/** PRIVATE
*  Makes sure that the value that is passed in does not have a value of "undefined"
*
**/

CalendarEvent.notUndefined = function( Text )
{
   if ( Text == undefined ) {
      return( "" );
   }
   else
      return( Text );
}


/** PACKAGE STATIC
*   CalendarEvent makeFromMcalEvent.
* 
* NOTES
*   Factory method for creating a calendar event from an mcal event.
*
*   MCal properties:
*   Category: [STRING] The category of the event.
*   Title: [STRING] The title of the event. 
*   Description: [DESCRIPTION] The description of the event.
*   Start: [ARRAY] an array of Minute, Hour, Day, Month, Year
*   End: [ARRAY] of Minute, Hour, Day, Month, Year
*   Alarm: [INT] The number of minutes before the event for an alarm to go off., 0 - NO ALARM
*   Class: [INT] 1 for private, or 0 for public.
*   Location: [STRING] The location of the event.
*   Repeat: [INT] 0 - no repeat, 1 - repeat daily, 2 - repeat weekly, 3- repeat monthly, 4 - repeat monthly on a day of week, 5 - repeat yearly
*   RepeatDate: [ARRAY] of EndDay, Month, Year
*   RecurInterval: [INT] - Interval of recurrence.
*   DaysToRepeat: [INT] - useful only with repeat weekly. You can choose the days to repeat the event on...
*   Sunday - 1, Monday - 2, Tuesday 4, Wednesday 8, Thursday 16, Friday 32, Saturday 64
*/

CalendarEvent.makeFromMcalEvent = function( mCalLibEvent, calendarEvent )
{
   if ( mCalLibEvent ) 
   {
       calendarEvent.id           = mCalLibEvent.Id;
       calendarEvent.title        = this.notUndefined( mCalLibEvent.Title );
       calendarEvent.description  = this.notUndefined( mCalLibEvent.Description );
       calendarEvent.category     = mCalLibEvent.Category;
       calendarEvent.location     = this.notUndefined( mCalLibEvent.Location );
       calendarEvent.privateEvent = mCalLibEvent.PrivateEvent;
       calendarEvent.allDay       = mCalLibEvent.AllDay;
       calendarEvent.lastModified = mCalLibEvent.LastModified;

       var startDateStr           = mCalLibEvent.GetStartDate();
       calendarEvent.start        = CalendarEvent.convertTimeFromMcal( startDateStr );
      
       //this is used instead of start because of repeating events.

       calendarEvent.displayDate  = CalendarEvent.convertTimeFromMcal( startDateStr );
              
       // end
       var endDateStr             = mCalLibEvent.GetEndDate();   
      
       if( endDateStr && endDateStr != ""  )
       {
           calendarEvent.end = CalendarEvent.convertTimeFromMcal( endDateStr );
       }
       else
       {
           calendarEvent.end = new Date( calendarEvent.start );
           calendarEvent.end.setHours( calendarEvent.end.getHours() + 1 );
       }
      
       // ALARM 
       
       calendarEvent.alarm              = mCalLibEvent.Alarm;
       calendarEvent.alarmWentOff       = mCalLibEvent.AlarmWentOff;
       calendarEvent.snoozeTime         = Number( mCalLibEvent.SnoozeTime );
       calendarEvent.inviteEmailAddress = mCalLibEvent.InviteEmailAddress;
       calendarEvent.alarmEmailAddress  = mCalLibEvent.AlarmEmailAddress;
      
       debug( "mcalEvent.attrlist.alarmEmailAddress is is "+mCalLibEvent.AlarmEmailAddress );
       
       var alarmLengthMins = mCalLibEvent.AlarmLength;
       
       if( calendarEvent.alarm )
       {
           if( alarmLengthMins % kDate_MinutesInDay == 0 )
           {
               // if even number of days use days
               calendarEvent.alarmLength    = alarmLengthMins / kDate_MinutesInDay;
               calendarEvent.alarmUnits     = CalendarEvent.kAlarmUnit_days;
           }
           else if( alarmLengthMins % kDate_MinutesInHour == 0 )
           {
               // if even number of hours use hours
               calendarEvent.alarmLength    = alarmLengthMins / kDate_MinutesInHour;
               calendarEvent.alarmUnits     = CalendarEvent.kAlarmUnit_hours;
           }
           else
           {
               //  use minutes
               calendarEvent.alarmLength     = alarmLengthMins;
               calendarEvent.alarmUnits      = CalendarEvent.kAlarmUnit_minutes;
           }
       }
       else
       {
           // alarm is off - use defaults
           calendarEvent.alarmLength     = 15;
           calendarEvent.alarmUnits      = CalendarEvent.kAlarmUnit_minutes;
       }
       var alarmTimeInMS = calendarEvent.calculateAlarmTime( );
              
       // RECUR 
       
       //calendarEvent.repeat              = mCalLibEvent.Repeat;
       calendarEvent.repeat               = false;
       calendarEvent.recurInterval       = mCalLibEvent.RecurInterval;
       calendarEvent.repeatForever       = mCalLibEvent.RepeatForever;
       calendarEvent.recurType           = mCalLibEvent.RecurType;
       //calendarEvent.repeatDayInMonth    = false;
       
       if( calendarEvent.recurType != 0 && calendarEvent.recurType != undefined ) {
           calendarEvent.repeat = true;
           calendarEvent.recurInterval   = mCalLibEvent.RecurInterval;
       }
       
       calendarEvent.recurType = mCalLibEvent.RecurType;

       switch( calendarEvent.recurType )
       {
           case 1:  
              if ( calendarEvent.recurInterval %7 == 0 ) 
              {
                  calendarEvent.repeatUnits   = this.kRepeatUnit_weeks;
                  calendarEvent.recurInterval = calendarEvent.recurInterval / 7;
              }
              else
              {
                  calendarEvent.repeatUnits = CalendarEvent.kRepeatUnit_days;
              }
                      
            break;
           case 2:  calendarEvent.repeatUnits = CalendarEvent.kRepeatUnit_weeks;       break;
           case 3:  calendarEvent.repeatUnits = CalendarEvent.kRepeatUnit_months;      break;
           case 4:  calendarEvent.repeatUnits = CalendarEvent.kRepeatUnit_months_day;  break;   
           case 5:  calendarEvent.repeatUnits = CalendarEvent.kRepeatUnit_years;       break;
           default: calendarEvent.repeatUnits = CalendarEvent.kRepeatUnit_days;        break;
       }

       var repeatEndDateStr = mCalLibEvent.GetRecurEndDate();

       if( repeatEndDateStr && repeatEndDateStr != "" )
       {
          calendarEvent.recurEnd    = CalendarEvent.convertDateFromMcal( repeatEndDateStr );
       }
       else
       {
         calendarEvent.recurEnd    = new Date();
       } 

       if ( calendarEvent.repeatForever ) 
       {
          calendarEvent.recurEnd   = new Date( 2100, 1, 1, 0, 0, 0 );
       }

      /** MP: Add this for finding a spot to put the event in the day view.
      *
      */
      calendarEvent.OtherSpotArray = new Array();
      calendarEvent.CurrentSpot = 0;
   }
}

/* PRIVATE static helper for mcal conversion */

CalendarEvent.convertTimeFromMcal = function ( time )
{
    var timeArray = time.split( "," );
    return new Date( timeArray[0], timeArray[1] - 1, timeArray[2], timeArray[3], timeArray[4], 0 );
}

/* PRIVATE static helper for mcal conversion */

CalendarEvent.convertDateFromMcal = function (  date, properMonths )
{
    
   var dateArray = date.split( "," );
   if ( properMonths ) 
      return new Date( dateArray[0], dateArray[1], dateArray[2] );
   else
      return new Date( dateArray[0], dateArray[1] - 1, dateArray[2] );
   
}


/* PRIVATE static helper for mcal conversion */

CalendarEvent.convertTimeToMcal = function( time )
{
   var mcalTime = new Object();
   
   mcalTime.year = time.getFullYear();
   mcalTime.month = time.getMonth() + 1;
   mcalTime.mday = time.getDate();
   mcalTime.hour = time.getHours();
   mcalTime.min = time.getMinutes();
   
   return mcalTime;

}

/* PRIVATE static helper for mcal conversion */

CalendarEvent.convertDateToMcal = function( date )
{
   var mcalDate = new Object();
   
   mcalDate.year = date.getFullYear();
   mcalDate.month = date.getMonth() + 1;
   mcalDate.mday = date.getDate();
   
   return mcalDate;

}

/** PACKAGE
*   CalendarEvent convertToMcalEvent.
* 
* NOTES
*   Convert a Calendar event to an mcal event, actual work for this is done in PHP
*/


CalendarEvent.prototype.convertToMcalEvent = function( )
{
	var mCalLibEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
	
   var mCalLibEvent = mCalLibEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
   	
   mCalLibEvent.Id             = this.id;
 	mCalLibEvent.Title          = this.title;
	mCalLibEvent.Description    = this.description;
	mCalLibEvent.Location       = this.location;
	mCalLibEvent.Category       = this.category;
   mCalLibEvent.PrivateEvent   = this.privateEvent;
	mCalLibEvent.AllDay         = this.allDay;
    
   var mcalStartDate = CalendarEvent.convertTimeToMcal( this.start );
	mCalLibEvent.SetStartDate( mcalStartDate.year, mcalStartDate.month, mcalStartDate.mday, mcalStartDate.hour, mcalStartDate.min );
	
   var mcalEndDate = CalendarEvent.convertTimeToMcal( this.end );
	mCalLibEvent.SetEndDate( mcalEndDate.year, mcalEndDate.month, mcalEndDate.mday, mcalEndDate.hour, mcalEndDate.min );
	
   mCalLibEvent.Alarm               = this.alarm;
	//mCalLibEvent.AlarmWentOff        = this.alarmWentOff;
	mCalLibEvent.AlarmEmailAddress   = this.alarmEmailAddress;
	mCalLibEvent.InviteEmailAddress  = this.inviteEmailAddress;
	if( this.snoozeTime )
      mCalLibEvent.SnoozeTime          = this.snoozeTime;

   switch( this.alarmUnits )
   {
      case CalendarEvent.kAlarmUnit_days    : 
         mCalLibEvent.AlarmLength = this.alarmLength * kDate_MinutesInDay;   
         break;
      case CalendarEvent.kAlarmUnit_hours   : 
         mCalLibEvent.AlarmLength = this.alarmLength * kDate_MinutesInHour;  
         break;
      case CalendarEvent.kAlarmUnit_minutes : 
         mCalLibEvent.AlarmLength = this.alarmLength;                        
         break;
   }
    
   this.recurType = 0;
   
   if( this.repeat ) {
      if( this.repeatUnits == ( CalendarEvent.kRepeatUnit_days ) )
      {
         this.recurType = 1;
      }
      else if( this.repeatUnits == ( CalendarEvent.kRepeatUnit_weeks ) )
      {
         //this.recurType = 2;
         this.recurType = 1;
         this.recurInterval = 7 * this.recurInterval;
      }
      else if( this.repeatUnits == ( CalendarEvent.kRepeatUnit_months ) )
         this.recurType = 3;
      else if( this.repeatUnits == ( CalendarEvent.kRepeatUnit_months_day ) )
         this.recurType = 4;
      else if( this.repeatUnits == ( CalendarEvent.kRepeatUnit_years ) )
         this.recurType = 5;
   }
    
   if ( this.repeatForever ) 
   {
      var mcalRecurEndDate = new Object();
      
      mcalRecurEndDate.year  = 0;
      
      mcalRecurEndDate.month = 0;
      
      mcalRecurEndDate.mday  = 0;
   }
   else
   {
      var mcalRecurEndDate = CalendarEvent.convertDateToMcal( this.recurEnd );
   }

   //mCalLibEvent.Repeat              = this.repeat;
   mCalLibEvent.RecurInterval       = this.recurInterval;
   mCalLibEvent.RepeatUnits         = this.repeatUnits;
   mCalLibEvent.RepeatForever       = this.repeatForever;
   
   mCalLibEvent.SetRecurInfo( this.recurType, mCalLibEvent.RecurInterval, mcalRecurEndDate.year, mcalRecurEndDate.month, mcalRecurEndDate.mday );

   return mCalLibEvent;
}

function debug( Text )
{
   dump( "\n"+ Text + "\n");
}
