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




// The calendar data source uses an observer pattern to notify clients when things change
// in the data source. The datasource calls back into the observer routines asynchrounously
// In order to keep the tests running we use async test objects.

// keep track of when the calendar data source has loaded. It only has to load once, since
// it is a singleton, so the loaded flag is global to all the tests

var loaded = false;

 

var gFileUtils = new FileUtils()

function setUp() 
{
    gFileUtils.remove( "/tmp/.oecalendar" );
    gFileUtils.remove( "/tmp/.oecalendar.bak" );
}

function tearDown() 
{
    gFileUtils.remove( "/tmp/.oecalendar" );
    gFileUtils.remove( "/tmp/.oecalendar.bak" );
}




// test validating a CalendarEvent.

function testValidateClass() 
{
   // calendar events that are not instances of CalendarEvent are illegal
   
   var ce = new Object();
   
   var exception = null;
   try
   {
      ce.verifyEvent(  );
   }
   catch( e )
   {
       exception = e;
   }
   
   // should throw an exception
   
   assertNotNull( exception );
}

// test validating missing fields.

function testValidateMissing() 
{
   // see that missing fields are validated
   
   var ce = CalendarEvent.makeNewEvent( new Date(), false );
   ce.start = null;
   
   var result = ce.verifyEvent(  );
   
   assertEquals( "testValidateMissing: invalid len", 0, result.invalid.length  );
   assertEquals( "testValidateMissing: len", 2, result.missing.length );
   assertEquals( "testValidateMissing: title", "title", result.missing[0]  );
   assertEquals( "testValidateMissing: start" , "start", result.missing[1] );
}

// test validating units.

function testValidateUnits() 
{
   // see that missing fields are validated
   
   var ce = CalendarEvent.makeNewEvent( new Date(), false );
   ce.title = "ho";
   
   ce.repeatUnits = "yow";
   ce.alarmUnits = "yow2";
   
   var result = ce.verifyEvent(  );
      
   assertEquals( "testValidateUnits: missing len" , 0, result.missing.length );
   assertEquals( "testValidateUnits: invalid len" , 2, result.invalid.length );
   assertEquals( "testValidateUnits: alarmUnits" , "alarmUnits", result.invalid[0].name );
   assertEquals( "testValidateUnits: repeatUnits" , "repeatUnits", result.invalid[1].name );
   

   ce.repeatUnits = CalendarEvent.kRepeatUnit_days;
   ce.alarmUnits = CalendarEvent.kAlarmUnit_minutes;
   
   result = ce.verifyEvent(  );
      
   assertEquals( "testValidateUnits: 1 units" , null, result );
   
   ce.repeatUnits = CalendarEvent.kRepeatUnit_weeks;
   ce.alarmUnits = CalendarEvent.kAlarmUnit_hours;
   
   result = ce.verifyEvent(  );
      
   assertEquals( "testValidateUnits: 2 units" , null, result );
   
   ce.repeatUnits = CalendarEvent.kRepeatUnit_months;
   ce.alarmUnits = CalendarEvent.kAlarmUnit_days;
   
   result = ce.verifyEvent(  );
      
   assertEquals( "testValidateUnits: 3 units" , null, result );
   
   ce.repeatUnits = CalendarEvent.kRepeatUnit_months_day;
   ce.alarmUnits = CalendarEvent.kAlarmUnit_hours;
   
   result = ce.verifyEvent(  );
      
   assertEquals( "testValidateUnits: 4 units" , null, result );
   
   ce.repeatUnits = CalendarEvent.kRepeatUnit_years;
   ce.alarmUnits = CalendarEvent.kAlarmUnit_hours;
   
   result = ce.verifyEvent(  );
      
   assertEquals( "testValidateUnits: 5 units" , null, result );
   
}

// test alarm checking

function testCheckAlarm() 
{
   
   var now = new Date();
   
   // make an event that starts in 15 min
   
   var in15Min = new Date( now );
   in15Min.setMinutes( now.getMinutes() + 15 );
   
   var ce = CalendarEvent.makeNewEvent( in15Min , false );
   ce.title = "ho";
   
   // give it an alarm of 20 mins before
   
   ce.setAlarmMinutes( 20 );
   var alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 15, 20 min before" , true, alarm );
   
   // give it an alarm of 15 mins before
   
   ce.setAlarmMinutes( 15 );
   var alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 15, 15 min before" , true, alarm );
   
   // give it an alarm of 10 mins before
   
   ce.setAlarmMinutes( 10 );
   alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 15, 10 min before" , false, alarm );
   
   
   
   
   // make an event that starts in 2 hours
   
   var in2Hours = new Date( now );
   in2Hours.setHours( now.getHours() + 2 );
   
   ce = CalendarEvent.makeNewEvent( in2Hours , false );
   ce.title = "ho";
   
   // give it an alarm of 3 hr before
   
   ce.setAlarmHours( 3 );
   alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 2 hours, 3 hr before" , true, alarm );
   
   // give it an alarm of 2 hr before
   
   ce.setAlarmHours( 2 );
   alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 2 hours, 2 hr before" , true, alarm );
   
   // give it an alarm of 1 hour before
   
   ce.setAlarmHours( 1 );
   var alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 2 hours, 1 hr before" , false, alarm );
   
   
   
   
   // make an event that starts in 2 days
   
   var in2days = new Date( now );
   in2days.setDate( now.getDate() + 2 );
   
   ce = CalendarEvent.makeNewEvent( in2days , false );
   ce.title = "ho";
   
   // give it an alarm of 3 days before
   
   ce.setAlarmDays( 3 );
   alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 2 days, 3 days before" , true, alarm );
   
   // give it an alarm of 3 days before
   
   ce.setAlarmDays( 2 );
   alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 2 days, 2 days before" , true, alarm );
   
   // give it an alarm of 1 hour before
   
   ce.setAlarmDays( 1 );
   var alarm = ce.checkAlarm();
   
   assertEquals( "testCheckAlarm: in 2 days, 1 day before" , false, alarm );
   
   
   
}


function testAdd( ) 
{
    var calendarObserver =
    {
        onLoad   : function()
        {
        },
        
        
        onAddItem : function( calendarEvent )
        {
        },
        
        onDeleteItem : function( calendarEvent )
        {
        }
        
        
    };
      
    var ds = new CalendarEventDataSource( calendarObserver, "/tmp/" );
    
    
    var startDate = new Date( 1959, 5, 5, 3, 3, 0 );
    var newEvent = CalendarEvent.makeNewEvent(  startDate , false );
    newEvent.title = "testEvent1";
    
    ds.addEvent( newEvent );
    
    dump( newEvent.toSource() );
     
}
// Test basic adding and deleting - always clean up your own test data

function testAddDelete( owner ) 
{
   
   testAddDeleteObject = new AsyncTestObject( "testAddDeleteObject", owner );
   
   testAddDeleteObject.startASync = function()
   {     
      this.done = false;
      this.addedEvent = null;
      this.deletedEvent = null;

      // the observer observer
      
      this.calendarObserver =
      {
        onLoad   : function()
        {
           testAddDeleteObject.loaded = true;
        },
      
       
        onAddItem : function( calendarEvent )
        {
            testAddDeleteObject.addedEvent = calendarEvent; 
            testAddDeleteObject.done = true;          
        },
      
        onDeleteItem : function( calendarEvent )
        {
            testAddDeleteObject.deletedEvent = calendarEvent; 
            testAddDeleteObject.done = true;          
        }
      
      
      };
      
      this.ds = new CalendarEventDataSource( this.calendarObserver, "/tmp/" );
      
   }
   
   testAddDeleteObject.startASync.message = "testAddDelete: Starting";
   testAddDeleteObject.startASync.done = function() 
   { 
      return this.loaded;
   }
   
   
      
   testAddDeleteObject.addEvent = function()
   {
      // ADD
      
      this.startDate = new Date( 1959, 5, 5, 3, 3, 0 );
      this.newEvent = CalendarEvent.makeNewEvent(  this.startDate , false );
      this.newEvent.title = "testEvent1";
      this.newEvent.snoozeTime  = 2200000001;  
      
      this.ds.addEvent( this.newEvent );
   }
   testAddDeleteObject.addEvent.message = "testAddDelete: Add Event";
   testAddDeleteObject.addEvent.done = function() 
   { 
      return this.done;
   }
   
      
   testAddDeleteObject.deleteEvent = function()
   {
      // DELETE
      
      this.done = false;
      
      this.ds.deleteEvent( this.addedEvent );
   }
   testAddDeleteObject.deleteEvent.message = "testAddDelete: Delete Event";
   testAddDeleteObject.deleteEvent.done = function() 
   { 
      return this.done;
   } 
   
      
   testAddDeleteObject.finish = function()
   {
      // stop observing
      
      this.ds.removeObserver( this.calendarObserver );
      
      // make sure the new event is right
      
      assertEquals( "add: title" , "testEvent1", this.addedEvent.title );
      assertEquals( "add: allDay" , false, this.addedEvent.allDay );
      assertEquals( "add: start" , this.startDate.toString(), this.addedEvent.start.toString() );
      assertEquals( "add: snoozeTime" , 2200000001, this.addedEvent.snoozeTime );
      
      // make sure event was deleted
      
      assertEquals( "delete: title", "testEvent1", this.deletedEvent.title );
      assertEquals( "delete: id" , this.addedEvent.id, this.deletedEvent.id );
  }
   
   testAddDeleteObject.finish.message = "testAddDelete: Finish";
   testAddDeleteObject.finish.done = function() 
   { 
      return true;
   } 



  testAddDeleteObject.startASync.next   = testAddDeleteObject.addEvent;
  testAddDeleteObject.addEvent.next     = testAddDeleteObject.deleteEvent;
  testAddDeleteObject.deleteEvent.next  = testAddDeleteObject.finish;
  testAddDeleteObject.finish.next       = null;
  
  
  
  testAddDeleteObject.runAsynch();
   
}












function testModify( owner ) 
{
   
   testModifyObject = new AsyncTestObject( "testModifyObject", owner );
   
   testModifyObject.startASync = function()
   {     
      this.done = false;
      this.addedEvent = null;
      this.deletedEvent = null;
      this.eventId = null;
      this.originalEvent = null;
  
      this.testNewEventObserver =
      {
        onLoad   : function()
        {
           testModifyObject.loaded = true;
        },
      
       
        onAddItem : function( calendarEvent )
        {
           // dump( "\n\nadd: " + calendarEvent.toSource() );
           testModifyObject.addedEvent = calendarEvent; 
           testModifyObject.eventId = calendarEvent.id;
           testModifyObject.done = true;          
        },
      
        onModifyItem : function( calendarEvent, originalEvent )
        {
            //dump( "\n\norg: " + originalEvent.toSource() );
            //dump( "\n\nmod: " + calendarEvent.toSource() );
           testModifyObject.originalEvent = originalEvent; 
           testModifyObject.modEvent = calendarEvent; 
           testModifyObject.done = true;  
        },
      
        onDeleteItem : function( calendarEvent )
        {
           testModifyObject.deletedEvent = calendarEvent; 
           testModifyObject.done = true;  
        }
      };
      
      
      this.ds = new CalendarEventDataSource( this.testNewEventObserver, "/tmp/" );
   };
   
   testModifyObject.startASync.message = "testModify: Starting";
   testModifyObject.startASync.done = function() 
   { 
      return this.loaded;
   }
      
      
      
      
   testModifyObject.addEvent = function()
   {
      // ADD
      
      this.startDate = new Date( 1959, 5, 5, 3, 3, 0 );
      this.newEvent = CalendarEvent.makeNewEvent(  this.startDate , false );
      this.newEvent.title = "testModifyEvent1";
      
      this.ds.addEvent( this.newEvent );
   } 
      
   testModifyObject.addEvent.message = "testModify: Add Event";
   testModifyObject.addEvent.done = function() 
   { 
      return this.done;
   }
     
     
     
   testModifyObject.modifyEvent = function()
   {
      
      // make sure the new event is right
      
      assertEquals( "add: title" , "testModifyEvent1", this.addedEvent.title );
      assertEquals( "add: allDay" , false, this.addedEvent.allDay );
      assertEquals( "add: start" , this.startDate.toString(), this.addedEvent.start.toString() );
      
      
      // MODIFY 
      
      this.addedEvent.title = "testModifyEvent2";
      this.newStart = new Date( 1958, 4, 4, 1, 2, 0 );
      this.addedEvent.start = this.newStart;
      this.newEnd = new Date( 1958, 4, 4, 1, 2, 0 );
      this.addedEvent.end = this.newEnd;
      
      this.addedEvent.description = "test description";
      this.addedEvent.location    = "test location";
	  this.addedEvent.inviteEmailAddress = "invite@em";
      this.addedEvent.alarmEmailAddress  = "alarm@em";
      
      this.addedEvent.privateEvent = false;
      
      this.addedEvent.alarm       = true;
      this.addedEvent.alarmLength = "11";
      this.addedEvent.alarmUnits  = CalendarEvent.kAlarmUnit_hours;  
      this.addedEvent.snoozeTime  = 2000000000;  
     
      this.addedEvent.repeat         = true;
      this.addedEvent.repeatInterval = "2";
      this.addedEvent.repeatUnits    = CalendarEvent.kRepeatUnit_days; 
      this.addedEvent.repeatForever  = false;
      this.repeatEnd = new Date( 2000, 6, 7, 0, 0, 0 );
      this.addedEvent.repeatEnd      = this.repeatEnd;
      
      this.addedEvent.category = "7";
      
      /* THESE FIELDS ARE NOT SET UP PROPERLY WHEN YOU GET AN EVENT
         FOR MODIFICATION!!!!
            category:null, 
            location:null, 
            inviteEmailAddress:null, 
            alarmEmailAddress:null, 
            alarmLength:15, 
            recurInterval:1, 
      */
      this.done = false; 
      this.ds.modifyEvent( this.addedEvent );
   }
      
   testModifyObject.modifyEvent.message = "testModify: Modify Event";
   testModifyObject.modifyEvent.done = function() 
   { 
      return this.done;
   }
      
      
   testModifyObject.deleteEvent = function()
   {
      // DELETE
      
      this.done = false;
      
      this.ds.deleteEvent( this.addedEvent );
   }
       
   testModifyObject.deleteEvent.message = "testModify: Delete Event";
   testModifyObject.deleteEvent.done = function() 
   { 
      return this.done;
   } 
   
        
   testModifyObject.finish = function()
   {
      // stop observing
      
      this.ds.removeObserver( this.testNewEventObserver );
            
      // make sure the modified event is right
      
      
      assertEquals( "mod: title" , "testModifyEvent2", this.modEvent.title );
      assertEquals( "mod: allDay" , false, this.modEvent.allDay );
      assertEquals( "mod: start" , this.newStart.toString(), this.modEvent.start.toString() );
      assertEquals( "mod: end" , this.newEnd.toString(), this.modEvent.end.toString() );
      assertEquals( "mod: description" , "test description", this.modEvent.description );
      assertEquals( "mod: category" , "7", this.modEvent.category );
      assertEquals( "mod: location" , "test location", this.modEvent.location );
      assertEquals( "mod: privateEvent" , false, this.modEvent.privateEvent );
      assertEquals( "mod: alarm" , true, this.modEvent.alarm );
      assertEquals( "mod: alarmLength" , 11, this.modEvent.alarmLength );
      assertEquals( "mod: alarmUnits" , CalendarEvent.kAlarmUnit_hours, this.modEvent.alarmUnits );
      assertEquals( "mod: snoozeTime" , 2000000000, this.modEvent.snoozeTime );
      assertEquals( "mod: inviteEmailAddress" , "invite@em", this.modEvent.inviteEmailAddress );
      assertEquals( "mod: alarmEmailAddress" , "alarm@em", this.modEvent.alarmEmailAddress );
      
      assertEquals( "mod: title" , "testModifyEvent1", this.originalEvent.title );
      assertEquals( "mod: repeat" , true, this.modEvent.repeat );
      assertEquals( "mod: repeatInterval" , "2", this.modEvent.repeatInterval );
      assertEquals( "mod: repeatForever" , false, this.modEvent.repeatForever );
      assertEquals( "mod: repeatEnd" , this.repeatEnd.toString(), this.modEvent.repeatEnd.toString() );
      assertEquals( "mod: repeatUnits" , CalendarEvent.kRepeatUnit_days, this.modEvent.repeatUnits );
      
      // make sure event was deleted
      
      assertEquals( "delete: title", "testModifyEvent2", this.deletedEvent.title );
      assertEquals( "delete: id" , this.addedEvent.id, this.deletedEvent.id );
   }  
   
   testModifyObject.finish.message = "testModify: Finish";
   testModifyObject.finish.done = function() 
   { 
      return true;
   } 
   
   
  
  testModifyObject.startASync.next = testModifyObject.addEvent;
  testModifyObject.addEvent.next = testModifyObject.modifyEvent;
  testModifyObject.modifyEvent.next = testModifyObject.deleteEvent;
  testModifyObject.deleteEvent.next = testModifyObject.finish;
  testModifyObject.finish.next = null;
  
  
  
  testModifyObject.runAsynch();
   
      
}

// Test alarms


function testAlarm( owner ) 
{
  testAlarmObject = new AsyncTestObject( "testAlarmObject", owner );
  
  
  
  testAlarmObject.startASync = function()
  {     
     
     this.loaded = false;
     this.done = false;
     this.addedEvent = null;
     this.deletedEvent = null;
     this.alarmEvent = null;
     this.eventId = null;
     
     // the observer
          
     this.testNewEventObserver =
     {
        onLoad   : function()
        {
           testAlarmObject.loaded = true;
        },
      
       
        onAddItem : function( calendarEvent )
        {
            
            if( "testAlarmEvent1" == calendarEvent.title )
            {
               testAlarmObject.addedEvent = calendarEvent; 
               testAlarmObject.eventId = calendarEvent.id;
               testAlarmObject.done = true;  
            }        
        },
     
        onDeleteItem : function( calendarEvent )
        {
            
            if( testAlarmObject.eventId && testAlarmObject.eventId == calendarEvent.id )
            {
               testAlarmObject.deletedEvent = calendarEvent; 
               testAlarmObject.done = true;          
            }
        },
   
     
        onAlarm : function( calendarEvent )
        {
            
            if( testAlarmObject.eventId && testAlarmObject.eventId == calendarEvent.id )
            {
               testAlarmObject.alarmEvent = calendarEvent; 
            }
        },
   
     };
     
     
     this.ds = new CalendarEventDataSource( this.testNewEventObserver, "/tmp/" );
  };
  
  testAlarmObject.startASync.message = "testAlarm: Starting";
  
  testAlarmObject.startASync.done = function() 
  { 
      return this.loaded;
  }
  
  
  
  
  
  
  
  testAlarmObject.addEvent = function()
  {
     // ADD an event to start in 30 min with an alarm 1 hour before
     
     var startDate = new Date( );
     startDate.setMinutes( startDate.getMinutes() + 30 );
      
     this.newEvent = CalendarEvent.makeNewEvent(  startDate , false );
     this.newEvent.title = "testAlarmEvent1";
     
     this.newEvent.setAlarmHours( 1 );
     
     this.done = false;
     this.ds.addEvent( this.newEvent );     
  };
  
  testAlarmObject.addEvent.message = "testAlarm: Add";
  
  testAlarmObject.addEvent.done = function()
  { 
      return this.done;
  }
  
  
  
  
  
  testAlarmObject.waitForAlarm = function()
  {
  };
  
  testAlarmObject.waitForAlarm.message = "testAlarm: Wait for alarm";
  
  testAlarmObject.waitForAlarm.done = function() 
  { 
      return this.alarmEvent != null;
  }
  
  
  
  
  testAlarmObject.deleteEvent = function()
  {
      // DELETE
  
      this.done = false;
  
      this.ds.deleteEvent( this.addedEvent );
  };
  
  testAlarmObject.deleteEvent.message = "testAlarm: delete event";
  
  testAlarmObject.deleteEvent.done = function() 
  { 
      return this.done;
  }
  
  
  
  
  
  testAlarmObject.finish = function()
  {
  
     // stop observing
     
     this.ds.removeObserver( this.testNewEventObserver );
           
     // make sure the alarm went off
     
     assertEquals( "alarm: title", this.addedEvent.id, this.alarmEvent.id );
     
     
  };
  
  testAlarmObject.finish.message = "testAlarm: Finish";
  testAlarmObject.finish.done = function()
  { 
      return true;
  }
  
  
  
  testAlarmObject.startASync.next = testAlarmObject.addEvent;
  testAlarmObject.addEvent.next = testAlarmObject.waitForAlarm;
  testAlarmObject.waitForAlarm.next = testAlarmObject.deleteEvent;
  testAlarmObject.deleteEvent.next = testAlarmObject.finish;
  testAlarmObject.finish.next = null;
  
  
  
  testAlarmObject.runAsynch();
  
}




function testGetDay( owner )
{

   testGetDayObject = new AsyncTestObject( "testGetDayObject", owner );
   
   testGetDayObject.waitTime = 1000;   // force a longer wait so we do not get two events with the same id
   
   testGetDayObject.startASync = function()
   {     
      this.loaded = false;
      this.done = false;
      this.addedEventList = new Array();
      this.deletedEvent = null;
  
      this.calendarObserver =
      {
        onLoad   : function()
        {
           testGetDayObject.loaded = true;
        },
      
       
        onAddItem : function( calendarEvent )
        {
            testGetDayObject.addedEventList[ testGetDayObject.addedEventList.length ] =  calendarEvent ; 
            testGetDayObject.done = true;;          
        },
      
        onDeleteItem : function( calendarEvent )
        {
            testGetDayObject.deletedEvent = calendarEvent; 
            --testGetDayObject.done;          
        }
      };
      
      this.ds = new CalendarEventDataSource( this.calendarObserver, "/tmp/" );
      
      this.startDate1 = new Date( 1972, 5, 5, 3, 3, 0 );
      this.startDate2 = new Date( 1972, 5, 5, 4, 3, 0 );
      this.startDate3 = new Date( 1972, 5, 6, 5, 3, 0 );
      
   };
    
   testGetDayObject.startASync.message = "testGetDay: Starting";
   
   testGetDayObject.startASync.done = function() 
   { 
      return this.loaded;
   }
  
  
      
   testGetDayObject.deleteOld = function()
   {
      var evs = this.ds.getEventsForMonth( this.startDate1 );
           
      this.done = evs.length;
      
      for( var eIndex = 0; eIndex < evs.length; ++eIndex )
      {
         this.ds.deleteEvent( evs[ eIndex ] );
      }
   }
      
   testGetDayObject.deleteOld.message = "testGetDay: delete old";
   
   testGetDayObject.deleteOld.done = function()
   { 
      return this.done == 0;
   }


      
      
   testGetDayObject.addEvent1 = function()
   {
      this.done = false;
      
      this.newEvent1 = CalendarEvent.makeNewEvent(  this.startDate1 , false );
      this.newEvent1.title = "atestDayEvent1";
      this.newEvent1.description = "Test 55";
      this.ds.addEvent( this.newEvent1 );  
  };
  
  testGetDayObject.addEvent1.message = "testGetDay: Add 1/3";
  
  testGetDayObject.addEvent1.done = function()
  { 
      return this.done;
  }
  
       // ADD
      
      
   testGetDayObject.addEvent2 = function()
   {
      this.done = false;
      
      this.newEvent2 = CalendarEvent.makeNewEvent(  this.startDate2 , false );
      this.newEvent2.title = "atestDayEvent2";
      this.newEvent2.description = "Test 55";
      this.ds.addEvent( this.newEvent2 );  
  };
  
  testGetDayObject.addEvent2.message = "testGetDay: Add 2/3";
  
  testGetDayObject.addEvent2.done = function()
  { 
      return this.done;
  }
  
       // ADD
      
      
   testGetDayObject.addEvent3 = function()
   {
      this.done = false;
      
      this.newEvent3 = CalendarEvent.makeNewEvent(  this.startDate3 , false );
      this.newEvent3.title = "atestDayEvent3";
      this.newEvent3.description = "Test 56";
      this.ds.addEvent( this.newEvent3 );  
   };
   
   testGetDayObject.addEvent3.message = "testGetDay: Add 3/3";
   
   testGetDayObject.addEvent3.done = function()
   { 
      return this.done;
   }
  
      
   testGetDayObject.testResults = function()
   {
      
      this.evs1 = this.ds.getEventsForDay( this.startDate1 );
      this.evs2 = this.ds.getEventsForDay( this.startDate3 );
      
      this.evs = this.evs1.concat( this.evs2 );
      
      this.evsMonth = this.ds.getEventsForMonth( this.startDate1 );
   }
   
   testGetDayObject.testResults.message = "testGetDay: get Results";
   
   testGetDayObject.testResults.done = function()
   { 
      return true;
   }
  
      
   testGetDayObject.cleanUp = function()
   {
      this.done = this.evs.length;
      
      for( var eIndex = 0; eIndex < this.evs.length; ++eIndex )
      {
         this.ds.deleteEvent( this.evs[ eIndex ] );
      }
   }
   
   testGetDayObject.cleanUp.message = "testGetDay: clean up";
   
   testGetDayObject.cleanUp.done = function()
   { 
      return this.done == 0;
   }
  
      
      
      
      
      
      
   testGetDayObject.finish = function()
   {
      // stop observing
      
      this.ds.removeObserver( this.calendarObserver );
      
      // make sure the new events are right
      
      for( var eIndex = 1; eIndex < this.evs.length; ++eIndex )
      {
         if( this.evs[ eIndex ].id == this.evs[ eIndex - 1 ].id  )
         {
            alert( "Mcal has added two events with the same ID! Don't trust the results of the getday test" );
         }
      }
      
      assertEquals( "getday: num events" , 2, this.evs1.length );
      assertEquals( "getday: description0" , "Test 55", this.evs1[0].description );
      assertEquals( "getday: description1" , "Test 55", this.evs1[1].description );
      
      assertEquals( "getday: evsMonth num events" , 3, this.evsMonth.length );
      
   };
    
   testGetDayObject.finish.message = "testGetDay: finish";
   
   testGetDayObject.finish.done = function()
   { 
      return true;
   }     
      
      
  testGetDayObject.startASync.next = testGetDayObject.deleteOld;
  testGetDayObject.deleteOld.next = testGetDayObject.addEvent1;
  testGetDayObject.addEvent1.next = testGetDayObject.addEvent2;
  testGetDayObject.addEvent2.next = testGetDayObject.addEvent3;
  testGetDayObject.addEvent3.next = testGetDayObject.testResults;
  testGetDayObject.testResults.next = testGetDayObject.cleanUp;
  testGetDayObject.cleanUp.next = testGetDayObject.finish;
  testGetDayObject.finish.next = null;
  
  
  
  testGetDayObject.runAsynch();
      
  
 }




function testGetById( owner )
{
   testGetByIdObject = new AsyncTestObject( "testGetByIdObject", owner );
   
   testGetByIdObject.startASync = function()
   {     
      this.loaded = false;
      this.done = false;
      this.addedEvent = null;
      this.deletedEvent = null;
  
     this.calendarObserver =
     {
        onLoad   : function()
        {
           testGetByIdObject.loaded = true;
        },
      
       
        onAddItem : function( calendarEvent )
        {
            testGetByIdObject.addedEvent = calendarEvent; 
            testGetByIdObject.done = true;          
        },
     
        onDeleteItem : function( calendarEvent )
        {
            testGetByIdObject.deletedEvent = calendarEvent; 
            testGetByIdObject.done = true;          
        }
     };
     
     
     this.ds = new CalendarEventDataSource( this.calendarObserver, "/tmp/" );
   }
   
   testGetByIdObject.startASync.message = "testGetById: Starting";
   
   testGetByIdObject.startASync.done = function() 
   { 
      return this.loaded;
   }
  
   
   
   
   
   
   testGetByIdObject.addEvent = function()
   {
     this.done = false;
     
     this.startDate = new Date( 1975, 5, 5, 3, 3, 0 );
     this.newEvent = CalendarEvent.makeNewEvent(  this.startDate , false );
     this.newEvent.title = "testGetByIdEvent";
     this.ds.addEvent( this.newEvent );
   } 
   
   testGetByIdObject.addEvent.message = "testGetById: Add";
   
   testGetByIdObject.addEvent.done = function()
   { 
      return this.done;
   }
    
     
   testGetByIdObject.getEvents = function()
   {
      // get events
      
      this.ev = this.ds.getCalendarEventById( this.addedEvent.id );
   }
   
   testGetByIdObject.getEvents.message = "testGetById: GetEvents";
   
   testGetByIdObject.getEvents.done = function()
   { 
      return true;
   }
    
     
     
     
   testGetByIdObject.deleteEvent = function()
   {
     // DELETE
     
     this.done = false;
     
     this.ds.deleteEvent( this.ev );
   }
   
   testGetByIdObject.deleteEvent.message = "testGetById: delete";
   
   testGetByIdObject.deleteEvent.done = function()
   { 
      return this.done;
   }
    
      
     
   testGetByIdObject.finish = function()
   {
     // stop observing
     
     this.ds.removeObserver(  this.calendarObserver );
     
     // make sure the new events are right
     
     assert(  this.ev.id ==   this.addedEvent.id );
       
   }
     
   testGetByIdObject.finish.message = "testGetById: finish";
   
   testGetByIdObject.finish.done = function()
   { 
      return true;
   }
   
   
   
   
   
   
   
   
  testGetByIdObject.startASync.next = testGetByIdObject.addEvent;
  testGetByIdObject.addEvent.next = testGetByIdObject.getEvents;
  testGetByIdObject.getEvents.next = testGetByIdObject.deleteEvent;
  testGetByIdObject.deleteEvent.next = testGetByIdObject.finish;
  testGetByIdObject.finish.next = null;
  
  
  
  testGetByIdObject.runAsynch();
   
 }


function searchHelper( str, reg )
{
    str.search( reg );
    return RegExp.$1;
}


function testXml( ) 
{
    var ds = new CalendarEventDataSource( null, "/tmp/" );
    
    
    var startDate = new Date( 1959, 5, 5, 3, 3, 0 );
    var newEvent = CalendarEvent.makeNewEvent(  startDate , false );
    newEvent.title = "testEvent1";
    
    ds.addEvent( newEvent );
    
    var xmlDoc = ds.makeXmlDocument();
    
    var domSerializer = Components.classes["@mozilla.org/xmlextras/xmlserializer;1"].getService( Components.interfaces.nsIDOMSerializer );
    var content = domSerializer.serializeToString( xmlDoc );

    assertEquals( "testXML start",  "-333737820000", searchHelper( content, /start="(.*)" displayDate/ ) );
    assertEquals( "testXML allDay",  "false", searchHelper( content, /allDay="(.*)" title/ ) );
    assertEquals( "testXML lastModified",  "", searchHelper( content, /lastModified="(.*)" alarm=/ ) );
    assertEquals( "testXML privateEvent",  "true", searchHelper( content, /privateEvent="(.*)" lastModified/ ) );
    
    
     ds.writeXmlFile( "/tmp/oecalendar.xml" );
}


    function debug( str )
    {
       // dump( "\nOE-debug: " + str + "\n" );
    }




function getTestFunctionNames() 
{
   testFunctionNames = new Array( 
                                "testValidateClass",
                                "testValidateMissing",
                                "testValidateUnits",
                                "testGetById",
                                "testAddDelete",
                                "testGetDay",
                                "testCheckAlarm",
                                "testAlarm",
                                "testModify",
                                "testXml"
                                
                                
                              );

 /* testFunctionNames = new Array(   "testModify"
                              );
  
 */  
  return testFunctionNames;
}


