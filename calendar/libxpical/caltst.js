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
 * Contributor(s): Mostafa Hosseini (mostafah@oeone.com)
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

const DEFAULT_TITLE="Lunch Time";
const DEFAULT_DESCRIPTION = "Will be out for one hour";
const DEFAULT_LOCATION = "Restaurant";
const DEFAULT_CATEGORY = "Personal";
const DEFAULT_EMAIL = "mostafah@oeone.com";
const DEFAULT_PRIVATE = false;
const DEFAULT_ALLDAY = false;
const DEFAULT_ALARM = true;
const DEFAULT_ALARMUNITS = "minutes";
const DEFAULT_ALARMLENGTH = 5;
const DEFAULT_RECUR = true;
const DEFAULT_RECURINTERVAL = 7;
const DEFAULT_RECURUNITS = "days";
const DEFAULT_RECURFOREVER = true;

var iCalLib = null;

function Test()
{
   if( iCalLib == null ) {
      netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
      var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
      iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
   }
    
   iCalLib.setServer( "/tmp/.oecalendar" );
   iCalLib.Test();
   alert( "Test Successfull" );
}

function TestAll()
{
   netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
   if( iCalLib == null ) {
      var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
      iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
   }
   iCalLib.setServer( "/tmp/.oecalendar" );
   var id = TestAddEvent();
   var iCalEvent = TestFetchEvent( id );
   id = TestUpdateEvent( iCalEvent );
//    TestSearchEvent();
   TestDeleteEvent( id );
   TestRecurring();
   
   //Todo tests
   var id = TestAddTodo();
   var iCalTodo = TestFetchTodo( id );
   id = TestUpdateTodo( iCalTodo );
   TestDeleteTodo( id );
   TestFilterTodo( id );
   alert( "Test Successfull" );
}

function TestAddEvent()
{
    var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
    
    var iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);

    iCalEvent.id = 999999999;
    iCalEvent.title = DEFAULT_TITLE;
    iCalEvent.description = DEFAULT_DESCRIPTION;
    iCalEvent.location = DEFAULT_LOCATION;
    iCalEvent.category = DEFAULT_CATEGORY;
    iCalEvent.privateEvent = DEFAULT_PRIVATE;
    iCalEvent.allDay = DEFAULT_ALLDAY;
    iCalEvent.alarm = DEFAULT_ALARM;
    iCalEvent.alarmUnits = DEFAULT_ALARMUNITS;
    iCalEvent.alarmLength = DEFAULT_ALARMLENGTH;
    iCalEvent.alarmEmailAddress = DEFAULT_EMAIL;
    iCalEvent.inviteEmailAddress = DEFAULT_EMAIL;

    iCalEvent.recur = DEFAULT_RECUR;
    iCalEvent.recurInterval = DEFAULT_RECURINTERVAL;
    iCalEvent.recurUnits = DEFAULT_RECURUNITS;
    iCalEvent.recurForever = DEFAULT_RECURFOREVER;

    iCalEvent.start.year = 2001;
    iCalEvent.start.month = 10; //November
    iCalEvent.start.day = 1;
    iCalEvent.start.hour = 12;
    iCalEvent.start.minute = 24;

    iCalEvent.end.year = 2001;
    iCalEvent.end.month = 10; //November
    iCalEvent.end.day = 1;
    iCalEvent.end.hour = 13;
    iCalEvent.end.minute = 24;

    var snoozetime = new Date();
    iCalEvent.setSnoozeTime( snoozetime );

    var id = iCalLib.addEvent( iCalEvent );
    
    if( id == null )
       alert( "Invalid Id" );
    if( iCalEvent.title != DEFAULT_TITLE )
       alert( "Invalid Title" );
    if( iCalEvent.description != DEFAULT_DESCRIPTION )
       alert( "Invalid Description" );
    if( iCalEvent.location != DEFAULT_LOCATION )
       alert( "Invalid Location" );
    if( iCalEvent.category != DEFAULT_CATEGORY )
       alert( "Invalid Category" );
    if( iCalEvent.privateEvent != DEFAULT_PRIVATE )
       alert( "Invalid PrivateEvent Setting" );
    if( iCalEvent.allDay != DEFAULT_ALLDAY )
       alert( "Invalid AllDay Setting" );
    if( iCalEvent.alarm != DEFAULT_ALARM )
       alert( "Invalid Alarm Setting" );
    if( iCalEvent.alarmUnits != DEFAULT_ALARMUNITS )
       alert( "Invalid Alarm Units" );
    if( iCalEvent.alarmLength != DEFAULT_ALARMLENGTH )
       alert( "Invalid Alarm Length" );
    if( iCalEvent.alarmEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Alarm Email Address" );
    if( iCalEvent.inviteEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Invite Email Address" );
    if( iCalEvent.recur != DEFAULT_RECUR )
       alert( "Invalid Recur Setting" );
    if( iCalEvent.recurInterval != DEFAULT_RECURINTERVAL )
       alert( "Invalid Recur Interval" );
    if( iCalEvent.recurUnits != DEFAULT_RECURUNITS )
       alert( "Invalid Recur Units" );
    if( iCalEvent.recurForever != DEFAULT_RECURFOREVER )
       alert( "Invalid Recur Forever" );

    //TODO: Check for start and end date

    return id;
}

function TestFetchEvent( id )
{
    var iCalEvent = iCalLib.fetchEvent( id );
    if( id == null )
       alert( "Invalid Id" );
    if( iCalEvent.title != DEFAULT_TITLE )
       alert( "Invalid Title" );
    if( iCalEvent.description != DEFAULT_DESCRIPTION )
       alert( "Invalid Description" );
    if( iCalEvent.location != DEFAULT_LOCATION )
       alert( "Invalid Location" );
    if( iCalEvent.category != DEFAULT_CATEGORY )
       alert( "Invalid Category" );
    if( iCalEvent.privateEvent != DEFAULT_PRIVATE )
       alert( "Invalid PrivateEvent Setting" );
    if( iCalEvent.allDay != DEFAULT_ALLDAY )
       alert( "Invalid AllDay Setting" );
    if( iCalEvent.alarm != DEFAULT_ALARM )
       alert( "Invalid Alarm Setting" );
    if( iCalEvent.alarmUnits != DEFAULT_ALARMUNITS )
       alert( "Invalid Alarm Units" );
    if( iCalEvent.alarmLength != DEFAULT_ALARMLENGTH )
       alert( "Invalid Alarm Length" );
    if( iCalEvent.alarmEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Alarm Email Address" );
    if( iCalEvent.inviteEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Invite Email Address" );
    if( iCalEvent.recur != DEFAULT_RECUR )
       alert( "Invalid Recur Setting" );
    if( iCalEvent.recurInterval != DEFAULT_RECURINTERVAL )
       alert( "Invalid Recur Interval" );
    if( iCalEvent.recurUnits != DEFAULT_RECURUNITS )
       alert( "Invalid Recur Units" );
    if( iCalEvent.recurForever != DEFAULT_RECURFOREVER )
       alert( "Invalid Recur Forever" );

    //TODO: Check for start and end date

    return iCalEvent;
}

function TestUpdateEvent( iCalEvent )
{
    iCalEvent.title = DEFAULT_TITLE+"*NEW*";
    iCalEvent.description = DEFAULT_DESCRIPTION+"*NEW*";
    iCalEvent.location = DEFAULT_LOCATION+"*NEW*";
    iCalEvent.category = DEFAULT_CATEGORY+"*NEW*";
    iCalEvent.privateEvent = !DEFAULT_PRIVATE;
    iCalEvent.allDay = !DEFAULT_ALLDAY;
    iCalEvent.alarm = !DEFAULT_ALARM;

    iCalEvent.recur = !DEFAULT_RECUR;

    iCalEvent.start.year = 2002;
    iCalEvent.start.month = 11; //December
    iCalEvent.start.day = 2;
    iCalEvent.start.hour = 13;
    iCalEvent.start.minute = 25;

    iCalEvent.end.year = 2002;
    iCalEvent.end.month = 11; //December
    iCalEvent.end.day = 2;
    iCalEvent.end.hour = 14;
    iCalEvent.end.minute = 25;

    var id = iCalLib.modifyEvent( iCalEvent );
    
    if( id == null )
       alert( "Invalid Id" );
    if( iCalEvent.title != DEFAULT_TITLE+"*NEW*" )
       alert( "Invalid Title" );
    if( iCalEvent.description != DEFAULT_DESCRIPTION+"*NEW*" )
       alert( "Invalid Description" );
    if( iCalEvent.location != DEFAULT_LOCATION+"*NEW*" )
       alert( "Invalid Location" );
    if( iCalEvent.category != DEFAULT_CATEGORY+"*NEW*" )
       alert( "Invalid Category" );
    if( iCalEvent.privateEvent != !DEFAULT_PRIVATE )
       alert( "Invalid PrivateEvent Setting" );
    if( iCalEvent.allDay != !DEFAULT_ALLDAY )
       alert( "Invalid AllDay Setting" );
    if( iCalEvent.alarm != !DEFAULT_ALARM )
       alert( "Invalid Alarm Setting" );
    if( iCalEvent.recur != !DEFAULT_RECUR )
       alert( "Invalid Recur Setting" );

    //TODO check start and end dates

    return id;
}
/*
function TestSearchEvent()
{
    result = iCalLib.SearchBySQL( "SELECT * FROM VEVENT WHERE CATEGORIES = 'Personal'" );
    alert( "Result : " + result );
}*/

function TestDeleteEvent( id )
{
    iCalLib.deleteEvent( id );

    var iCalEvent = iCalLib.fetchEvent( id );

    if( iCalEvent != null )
       alert( "Delete failed" );
}

function TestRecurring() {
   var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
   this.iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);

   iCalEvent.allDay = true;
   iCalEvent.recur = true;
   iCalEvent.recurInterval = 1;
   iCalEvent.recurUnits = "years";
   iCalEvent.recurForever = true;

   iCalEvent.start.year = 2001;
   iCalEvent.start.month = 0;
   iCalEvent.start.day = 1;
   iCalEvent.start.hour = 0;
   iCalEvent.start.minute = 0;

   iCalEvent.end.year = 2001;
   iCalEvent.end.month = 0;
   iCalEvent.end.day = 1;
   iCalEvent.end.hour = 23;
   iCalEvent.end.minute = 59;

   var id = iCalLib.addEvent( iCalEvent );

   var displayDates =  new Object();
   var checkdate = new Date( 2002, 0, 1, 0, 0, 0 );
   var eventList = iCalLib.getEventsForDay( checkdate, displayDates );

   if( !eventList.hasMoreElements() )
      alert( "Yearly Recur Test Failed" );
   
   var displayDate = new Date( displayDates.value.getNext().QueryInterface(Components.interfaces.nsISupportsPRTime).data );
   iCalLib.deleteEvent( id );
}

function TestAddTodo()
{
    var iCalTodoComponent = Components.classes["@mozilla.org/icaltodo;1"].createInstance();
    
    var iCalTodo = iCalTodoComponent.QueryInterface(Components.interfaces.oeIICalTodo);

    iCalTodo.id = 999999999;
    iCalTodo.title = DEFAULT_TITLE;
    iCalTodo.description = DEFAULT_DESCRIPTION;
    iCalTodo.location = DEFAULT_LOCATION;
    iCalTodo.category = DEFAULT_CATEGORY;
    iCalTodo.privateEvent = DEFAULT_PRIVATE;
    iCalTodo.allDay = DEFAULT_ALLDAY;
    iCalTodo.alarm = DEFAULT_ALARM;
    iCalTodo.alarmUnits = DEFAULT_ALARMUNITS;
    iCalTodo.alarmLength = DEFAULT_ALARMLENGTH;
    iCalTodo.alarmEmailAddress = DEFAULT_EMAIL;
    iCalTodo.inviteEmailAddress = DEFAULT_EMAIL;

    iCalTodo.recur = DEFAULT_RECUR;
    iCalTodo.recurInterval = DEFAULT_RECURINTERVAL;
    iCalTodo.recurUnits = DEFAULT_RECURUNITS;
    iCalTodo.recurForever = DEFAULT_RECURFOREVER;

    iCalTodo.start.year = 2001;
    iCalTodo.start.month = 10; //November
    iCalTodo.start.day = 1;
    iCalTodo.start.hour = 12;
    iCalTodo.start.minute = 24;

    iCalTodo.due.year = 2001;
    iCalTodo.due.month = 11; //December
    iCalTodo.due.day = 1;
    iCalTodo.due.hour = 23;
    iCalTodo.due.minute = 59;

    var id = iCalLib.addTodo( iCalTodo );
    
    if( id == null )
       alert( "Invalid Id" );
    if( iCalTodo.title != DEFAULT_TITLE )
       alert( "Invalid Title" );
    if( iCalTodo.description != DEFAULT_DESCRIPTION )
       alert( "Invalid Description" );
    if( iCalTodo.location != DEFAULT_LOCATION )
       alert( "Invalid Location" );
    if( iCalTodo.category != DEFAULT_CATEGORY )
       alert( "Invalid Category" );
    if( iCalTodo.privateEvent != DEFAULT_PRIVATE )
       alert( "Invalid PrivateEvent Setting" );
    if( iCalTodo.allDay != DEFAULT_ALLDAY )
       alert( "Invalid AllDay Setting" );
    if( iCalTodo.alarm != DEFAULT_ALARM )
       alert( "Invalid Alarm Setting" );
    if( iCalTodo.alarmUnits != DEFAULT_ALARMUNITS )
       alert( "Invalid Alarm Units" );
    if( iCalTodo.alarmLength != DEFAULT_ALARMLENGTH )
       alert( "Invalid Alarm Length" );
    if( iCalTodo.alarmEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Alarm Email Address" );
    if( iCalTodo.inviteEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Invite Email Address" );
    if( iCalTodo.recur != DEFAULT_RECUR )
       alert( "Invalid Recur Setting" );
    if( iCalTodo.recurInterval != DEFAULT_RECURINTERVAL )
       alert( "Invalid Recur Interval" );
    if( iCalTodo.recurUnits != DEFAULT_RECURUNITS )
       alert( "Invalid Recur Units" );
    if( iCalTodo.recurForever != DEFAULT_RECURFOREVER )
       alert( "Invalid Recur Forever" );

    //TODO: Check for start and end date

    return id;
}

function TestFetchTodo( id )
{
    var iCalEvent = iCalLib.fetchTodo( id );
    if( id == null )
       alert( "Invalid Id" );
    if( iCalEvent.title != DEFAULT_TITLE )
       alert( "Invalid Title" );
    if( iCalEvent.description != DEFAULT_DESCRIPTION )
       alert( "Invalid Description" );
    if( iCalEvent.location != DEFAULT_LOCATION )
       alert( "Invalid Location" );
    if( iCalEvent.category != DEFAULT_CATEGORY )
       alert( "Invalid Category" );
    if( iCalEvent.privateEvent != DEFAULT_PRIVATE )
       alert( "Invalid PrivateEvent Setting" );
    if( iCalEvent.allDay != DEFAULT_ALLDAY )
       alert( "Invalid AllDay Setting" );
    if( iCalEvent.alarm != DEFAULT_ALARM )
       alert( "Invalid Alarm Setting" );
    if( iCalEvent.alarmUnits != DEFAULT_ALARMUNITS )
       alert( "Invalid Alarm Units" );
    if( iCalEvent.alarmLength != DEFAULT_ALARMLENGTH )
       alert( "Invalid Alarm Length" );
    if( iCalEvent.alarmEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Alarm Email Address" );
    if( iCalEvent.inviteEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Invite Email Address" );
    if( iCalEvent.recur != DEFAULT_RECUR )
       alert( "Invalid Recur Setting" );
    if( iCalEvent.recurInterval != DEFAULT_RECURINTERVAL )
       alert( "Invalid Recur Interval" );
    if( iCalEvent.recurUnits != DEFAULT_RECURUNITS )
       alert( "Invalid Recur Units" );
    if( iCalEvent.recurForever != DEFAULT_RECURFOREVER )
       alert( "Invalid Recur Forever" );

    //TODO: Check for start and end date

    return iCalEvent;
}

function TestUpdateTodo( iCalTodo )
{
    iCalTodo.title = DEFAULT_TITLE+"*NEW*";
    iCalTodo.description = DEFAULT_DESCRIPTION+"*NEW*";
    iCalTodo.location = DEFAULT_LOCATION+"*NEW*";
    iCalTodo.category = DEFAULT_CATEGORY+"*NEW*";
    iCalTodo.privateEvent = !DEFAULT_PRIVATE;
    iCalTodo.allDay = !DEFAULT_ALLDAY;
    iCalTodo.alarm = !DEFAULT_ALARM;

    iCalTodo.recur = !DEFAULT_RECUR;

    iCalTodo.start.year = 2002;
    iCalTodo.start.month = 0; //January
    iCalTodo.start.day = 2;
    iCalTodo.start.hour = 13;
    iCalTodo.start.minute = 25;

    var id = iCalLib.modifyTodo( iCalTodo );
    
    if( id == null )
       alert( "Invalid Id" );
    if( iCalTodo.title != DEFAULT_TITLE+"*NEW*" )
       alert( "Invalid Title" );
    if( iCalTodo.description != DEFAULT_DESCRIPTION+"*NEW*" )
       alert( "Invalid Description" );
    if( iCalTodo.location != DEFAULT_LOCATION+"*NEW*" )
       alert( "Invalid Location" );
    if( iCalTodo.category != DEFAULT_CATEGORY+"*NEW*" )
       alert( "Invalid Category" );
    if( iCalTodo.privateEvent != !DEFAULT_PRIVATE )
       alert( "Invalid PrivateEvent Setting" );
    if( iCalTodo.allDay != !DEFAULT_ALLDAY )
       alert( "Invalid AllDay Setting" );
    if( iCalTodo.alarm != !DEFAULT_ALARM )
       alert( "Invalid Alarm Setting" );
    if( iCalTodo.recur != !DEFAULT_RECUR )
       alert( "Invalid Recur Setting" );

    //TODO check start and end dates

    return id;
}

function TestDeleteTodo( id )
{
    iCalLib.deleteTodo( id );

    var iCalEvent = iCalLib.fetchTodo( id );

    if( iCalEvent != null )
       alert( "Delete failed" );
}

function TestFilterTodo()
{
    var iCalTodoComponent = Components.classes["@mozilla.org/icaltodo;1"].createInstance();
    
    var iCalTodo = iCalTodoComponent.QueryInterface(Components.interfaces.oeIICalTodo);

    iCalTodo.id = 999999999;

    iCalTodo.start.year = 2002;
    iCalTodo.start.month = 0;
    iCalTodo.start.day = 1;
    iCalTodo.start.hour = 0;
    iCalTodo.start.minute = 0;

    iCalTodo.due.year = 2003;
    iCalTodo.due.month = 0;
    iCalTodo.due.day = 1;
    iCalTodo.due.hour = 0;
    iCalTodo.due.minute = 0;

    var id = iCalLib.addTodo( iCalTodo );

	var todoList = this.iCalLib.getAllTodos();

	if ( !todoList.hasMoreElements() ) {
		alert( "TestFilterTodo-Step1: failed" );
		return;
	}
	
	var now = Date();

	iCalLib.filter.completed.setTime( now );

	todoList = this.iCalLib.getAllTodos();

	if ( !todoList.hasMoreElements() ) {
		alert( "TestFilterTodo-Step2: failed" );
		return;
	}

	iCalTodo.completed.setTime( now );

	now = Date();
	iCalLib.filter.completed.setTime( now );

	todoList = this.iCalLib.getAllTodos();

	if ( todoList.hasMoreElements() ) {
		alert( "TestFilterTodo-Step3: failed" );
		return;
	}

	iCalTodo.completed.clear();

	todoList = this.iCalLib.getAllTodos();

	if ( !todoList.hasMoreElements() ) {
		alert( "TestFilterTodo-Step4: failed" );
		return;
	}

    iCalLib.deleteTodo( id );
}
