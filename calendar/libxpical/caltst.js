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

const DEFAULT_SERVER="file:///tmp/.oecalendar";
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
const DEFAULT_ATTACHMENT = DEFAULT_SERVER;

var iCalLib = null;
var gObserver = null;
var gTodoObserver = null;

function Observer() 
{
}

Observer.prototype.onAddItem = function( icalevent )
{
	dump( "Observer.prototype.onAddItem\n" );
}

Observer.prototype.onModifyItem = function( icalevent, oldevent )
{
	dump( "Observer.prototype.onModifyItem\n" );
}

Observer.prototype.onDeleteItem = function( icalevent )
{
	dump( "Observer.prototype.onDeleteItem\n" );
}

Observer.prototype.onAlarm = function( icalevent )
{
	dump( "Observer.prototype.onAlarm\n" );
}

Observer.prototype.onLoad = function()
{
	dump( "Observer.prototype.onLoad\n" );
}

Observer.prototype.onStartBatch = function()
{
	dump( "Observer.prototype.onStartBatch\n" );
}

Observer.prototype.onEndBatch = function()
{
	dump( "Observer.prototype.onEndBatch\n" );
}

function Test()
{
   netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
   if( iCalLib == null ) {
      var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
      iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
   }
    
   iCalLib.server = DEFAULT_SERVER;
   iCalLib.Test();
   alert( "Finished Test" );
}

function TestAll()
{
   netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
   if( iCalLib == null ) {
      var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
      iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
   }
   if( gObserver == null ) {
	   gObserver = new Observer();
	   iCalLib.addObserver( gObserver );
   }
   if( gTodoObserver == null ) {
	   gTodoObserver = new Observer();
	   iCalLib.addTodoObserver( gTodoObserver );
   }
   if( !TestTimeConversion() ) {
	   alert( "Stopped Test" );
	   return;
   }

   iCalLib.server = DEFAULT_SERVER;
   var id = TestAddEvent();
   if( id == 0 ) {
	   alert( "Stopped Test" );
	   return;
   }
   var iCalEvent = TestFetchEvent( id );
   if( iCalEvent == null ) {
	   alert( "Stopped Test" );
	   return;
   }
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
   TestIcalString();
   alert( "Finished Test" );
}

function TestTimeConversion() {
	netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
	var dateTimeComponent = Components.classes["@mozilla.org/oedatetime;1"].createInstance();
	dateTime = dateTimeComponent.QueryInterface(Components.interfaces.oeIDateTime);
	var date1 = new Date();
	dateTime.setTime( date1 );
	var date1inms = parseInt( date1.getTime()/1000 );
	if( (dateTime.getTime()/1000) != date1inms ) {
		alert( "TestTimeConversion(): Step1 failed" );
		return false;
	}
	date1 = new Date( 1970, 0, 1, 0, 0, 0 );
	dateTime.setTime( date1 );
	date1inms = parseInt( date1.getTime()/1000 );
	if( (dateTime.getTime()/1000) != date1inms ) {
		alert( "TestTimeConversion(): Step2 failed" );
		return false;
	}
	date1 = new Date( 1969, 11, 31, 23, 59, 59 );
	dateTime.setTime( date1 );
	date1inms = parseInt( date1.getTime()/1000 );
	if( (dateTime.getTime()/1000) != date1inms ) {
		alert( "TestTimeConversion(): Step3 failed" );
		return false;
	}
	date1 = new Date( 1969, 11, 31, 19, 0, 0 );
	dateTime.setTime( date1 );
	date1inms = parseInt( date1.getTime()/1000 );
	if( (dateTime.getTime()/1000) != date1inms ) {
		alert( "TestTimeConversion(): Step4 failed" );
		return false;
	}
	date1 = new Date( 1962, 7, 03, 0, 0, 0 );
	dateTime.setTime( date1 );
	date1inms = parseInt( date1.getTime()/1000 );
	if( (dateTime.getTime()/1000) != date1inms ) {
		alert( "TestTimeConversion(): Step5 failed" );
		return false;
	}
	if( dateTime.hour != date1.getHours() ) {
		alert( "TestTimeConversion(): Step6 failed" );
		return false;
	}
	return true;
}

function TestAddEvent()
{
    var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
    
    var iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);

    iCalEvent.title = DEFAULT_TITLE;
    iCalEvent.description = DEFAULT_DESCRIPTION;
    iCalEvent.location = DEFAULT_LOCATION;
    iCalEvent.categories = DEFAULT_CATEGORY;
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

	Attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance( Components.interfaces.nsIMsgAttachment );
	Attachment.url = DEFAULT_ATTACHMENT;
	iCalEvent.addAttachment( Attachment );

	Contact = Components.classes["@mozilla.org/addressbook/cardproperty;1"].createInstance( Components.interfaces.nsIAbCard );
	Contact.primaryEmail = DEFAULT_EMAIL;
	iCalEvent.addContact( Contact );

    var id = iCalLib.addEvent( iCalEvent );
    
    if( id == null ){
		alert( "TestAddEvent(): Invalid Id" );
		return 0;
	}
    if( iCalEvent.title != DEFAULT_TITLE ){
		alert( "TestAddEvent(): Invalid Title" );
		return 0;
	}
    if( iCalEvent.description != DEFAULT_DESCRIPTION ){
		alert( "TestAddEvent(): Invalid Description" );
		return 0;
	}
    if( iCalEvent.location != DEFAULT_LOCATION ){
		alert( "TestAddEvent(): Invalid Location" );
		return 0;
	}
    if( iCalEvent.categories != DEFAULT_CATEGORY ){
		alert( "TestAddEvent(): Invalid Category" );
		return 0;
	}
    if( iCalEvent.privateEvent != DEFAULT_PRIVATE ){
		alert( "TestAddEvent(): Invalid PrivateEvent Setting" );
		return 0;
	}
    if( iCalEvent.allDay != DEFAULT_ALLDAY ){
		alert( "TestAddEvent(): Invalid AllDay Setting" );
		return 0;
	}
    if( iCalEvent.alarm != DEFAULT_ALARM ){
		alert( "TestAddEvent(): Invalid Alarm Setting" );
		return 0;
	}
    if( iCalEvent.alarmUnits != DEFAULT_ALARMUNITS ){
		alert( "TestAddEvent(): Invalid Alarm Units" );
		return 0;
	}
    if( iCalEvent.alarmLength != DEFAULT_ALARMLENGTH ){
		alert( "TestAddEvent(): Invalid Alarm Length" );
		return 0;
	}
    if( iCalEvent.alarmEmailAddress != DEFAULT_EMAIL ){
		alert( "TestAddEvent(): Invalid Alarm Email Address" );
		return 0;
	}
    if( iCalEvent.inviteEmailAddress != DEFAULT_EMAIL ){
		alert( "TestAddEvent(): Invalid Invite Email Address" );
		return 0;
	}
    if( iCalEvent.recur != DEFAULT_RECUR ){
		alert( "TestAddEvent(): Invalid Recur Setting" );
		return 0;
	}
    if( iCalEvent.recurInterval != DEFAULT_RECURINTERVAL ){
		alert( "TestAddEvent(): Invalid Recur Interval" );
		return 0;
	}
    if( iCalEvent.recurUnits != DEFAULT_RECURUNITS ){
		alert( "TestAddEvent(): Invalid Recur Units" );
		return 0;
	}
    if( iCalEvent.recurForever != DEFAULT_RECURFOREVER ){
		alert( "TestAddEvent(): Invalid Recur Forever" );
		return 0;
	}

    //TODO: Check for start and end date

    return id;
}

function TestFetchEvent( id )
{
    var iCalEvent = iCalLib.fetchEvent( id );
    if( id == null ){
		alert( "TestFetchEvent(): Invalid Id" );
		return null;
	}
    if( iCalEvent.title != DEFAULT_TITLE ){
		alert( "TestFetchEvent(): Invalid Title" );
		return null;
	}
    if( iCalEvent.description != DEFAULT_DESCRIPTION ){
		alert( "TestFetchEvent(): Invalid Description" );
		return null;
	}
    if( iCalEvent.location != DEFAULT_LOCATION ){
		alert( "TestFetchEvent(): Invalid Location" );
		return null;
	}
    if( iCalEvent.categories != DEFAULT_CATEGORY ){
		alert( "TestFetchEvent(): Invalid Category" );
		return null;
	}
    if( iCalEvent.privateEvent != DEFAULT_PRIVATE ){
		alert( "TestFetchEvent(): Invalid PrivateEvent Setting" );
		return null;
	}
    if( iCalEvent.allDay != DEFAULT_ALLDAY ){
		alert( "TestFetchEvent(): Invalid AllDay Setting" );
		return null;
	}
    if( iCalEvent.alarm != DEFAULT_ALARM ){
		alert( "TestFetchEvent(): Invalid Alarm Setting" );
		return null;
	}
    if( iCalEvent.alarmUnits != DEFAULT_ALARMUNITS ){
		alert( "TestFetchEvent(): Invalid Alarm Units" );
		return null;
	}
    if( iCalEvent.alarmLength != DEFAULT_ALARMLENGTH ){
		alert( "TestFetchEvent(): Invalid Alarm Length" );
		return null;
	}
    if( iCalEvent.alarmEmailAddress != DEFAULT_EMAIL ){
		alert( "TestFetchEvent(): Invalid Alarm Email Address" );
		return null;
	}
    if( iCalEvent.inviteEmailAddress != DEFAULT_EMAIL ){
		alert( "TestFetchEvent(): Invalid Invite Email Address" );
		return null;
	}
    if( iCalEvent.recur != DEFAULT_RECUR ){
		alert( "TestFetchEvent(): Invalid Recur Setting" );
		return null;
	}
    if( iCalEvent.recurInterval != DEFAULT_RECURINTERVAL ){
		alert( "TestFetchEvent(): Invalid Recur Interval" );
		return null;
	}
    if( iCalEvent.recurUnits != DEFAULT_RECURUNITS ){
		alert( "TestFetchEvent(): Invalid Recur Units" );
		return null;
	}
    if( iCalEvent.recurForever != DEFAULT_RECURFOREVER ){
		alert( "TestFetchEvent(): Invalid Recur Forever" );
		return null;
	}
	if( !iCalEvent.attachmentsArray.Count() ) {
		alert( "TestFetchEvent(): No attachment found" );
		return null;
	}
	attachment = iCalEvent.attachmentsArray.QueryElementAt( 0, Components.interfaces.nsIMsgAttachment );
	if ( attachment.url != DEFAULT_ATTACHMENT ) {
		alert( "TestFetchEvent(): Invalid attachment" );
		return null;
	}
    //TODO: Check for start and end date

    return iCalEvent;
}

function TestUpdateEvent( iCalEvent )
{
    iCalEvent.title = DEFAULT_TITLE+"*NEW*";
    iCalEvent.description = DEFAULT_DESCRIPTION+"*NEW*";
    iCalEvent.location = DEFAULT_LOCATION+"*NEW*";
    iCalEvent.categories = DEFAULT_CATEGORY+"*NEW*";
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
       alert( "TestUpdateEvent(): Invalid Id" );
    if( iCalEvent.title != DEFAULT_TITLE+"*NEW*" )
       alert( "TestUpdateEvent(): Invalid Title" );
    if( iCalEvent.description != DEFAULT_DESCRIPTION+"*NEW*" )
       alert( "TestUpdateEvent(): Invalid Description" );
    if( iCalEvent.location != DEFAULT_LOCATION+"*NEW*" )
       alert( "TestUpdateEvent(): Invalid Location" );
    if( iCalEvent.categories != DEFAULT_CATEGORY+"*NEW*" )
       alert( "TestUpdateEvent(): Invalid Category" );
    if( iCalEvent.privateEvent != !DEFAULT_PRIVATE )
       alert( "TestUpdateEvent(): Invalid PrivateEvent Setting" );
    if( iCalEvent.allDay != !DEFAULT_ALLDAY )
       alert( "TestUpdateEvent(): Invalid AllDay Setting" );
    if( iCalEvent.alarm != !DEFAULT_ALARM )
       alert( "TestUpdateEvent(): Invalid Alarm Setting" );
    if( iCalEvent.recur != !DEFAULT_RECUR )
       alert( "TestUpdateEvent(): Invalid Recur Setting" );

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
   var eventList = iCalLib.getEventsForDay( checkdate );

   if( !eventList.hasMoreElements() )
      alert( "Yearly Recur Test Failed" );
   
   iCalLib.deleteEvent( id );
}

function TestAddTodo()
{
    var iCalTodoComponent = Components.classes["@mozilla.org/icaltodo;1"].createInstance();
    
    var iCalTodo = iCalTodoComponent.QueryInterface(Components.interfaces.oeIICalTodo);

    iCalTodo.title = DEFAULT_TITLE;
    iCalTodo.description = DEFAULT_DESCRIPTION;
    iCalTodo.location = DEFAULT_LOCATION;
    iCalTodo.categories = DEFAULT_CATEGORY;
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
       alert( "TestAddTodo(): Invalid Id" );
    if( iCalTodo.title != DEFAULT_TITLE )
       alert( "TestAddTodo(): Invalid Title" );
    if( iCalTodo.description != DEFAULT_DESCRIPTION )
       alert( "TestAddTodo(): Invalid Description" );
    if( iCalTodo.location != DEFAULT_LOCATION )
       alert( "TestAddTodo(): Invalid Location" );
    if( iCalTodo.categories != DEFAULT_CATEGORY )
       alert( "TestAddTodo(): Invalid Category" );
    if( iCalTodo.privateEvent != DEFAULT_PRIVATE )
       alert( "TestAddTodo(): Invalid PrivateEvent Setting" );
    if( iCalTodo.allDay != DEFAULT_ALLDAY )
       alert( "TestAddTodo(): Invalid AllDay Setting" );
    if( iCalTodo.alarm != DEFAULT_ALARM )
       alert( "TestAddTodo(): Invalid Alarm Setting" );
    if( iCalTodo.alarmUnits != DEFAULT_ALARMUNITS )
       alert( "TestAddTodo(): Invalid Alarm Units" );
    if( iCalTodo.alarmLength != DEFAULT_ALARMLENGTH )
       alert( "TestAddTodo(): Invalid Alarm Length" );
    if( iCalTodo.alarmEmailAddress != DEFAULT_EMAIL )
       alert( "TestAddTodo(): Invalid Alarm Email Address" );
    if( iCalTodo.inviteEmailAddress != DEFAULT_EMAIL )
       alert( "TestAddTodo(): Invalid Invite Email Address" );
    if( iCalTodo.recur != DEFAULT_RECUR )
       alert( "TestAddTodo(): Invalid Recur Setting" );
    if( iCalTodo.recurInterval != DEFAULT_RECURINTERVAL )
       alert( "TestAddTodo(): Invalid Recur Interval" );
    if( iCalTodo.recurUnits != DEFAULT_RECURUNITS )
       alert( "TestAddTodo(): Invalid Recur Units" );
    if( iCalTodo.recurForever != DEFAULT_RECURFOREVER )
       alert( "TestAddTodo(): Invalid Recur Forever" );

    //TODO: Check for start and end date

    return id;
}

function TestFetchTodo( id )
{
    var iCalEvent = iCalLib.fetchTodo( id );
    if( id == null )
       alert( "TestFetchTodo(): Invalid Id" );
    if( iCalEvent.title != DEFAULT_TITLE )
       alert( "TestFetchTodo(): Invalid Title" );
    if( iCalEvent.description != DEFAULT_DESCRIPTION )
       alert( "TestFetchTodo(): Invalid Description" );
    if( iCalEvent.location != DEFAULT_LOCATION )
       alert( "TestFetchTodo(): Invalid Location" );
    if( iCalEvent.categories != DEFAULT_CATEGORY )
       alert( "TestFetchTodo(): Invalid Category" );
    if( iCalEvent.privateEvent != DEFAULT_PRIVATE )
       alert( "TestFetchTodo(): Invalid PrivateEvent Setting" );
    if( iCalEvent.allDay != DEFAULT_ALLDAY )
       alert( "TestFetchTodo(): Invalid AllDay Setting" );
    if( iCalEvent.alarm != DEFAULT_ALARM )
       alert( "TestFetchTodo(): Invalid Alarm Setting" );
    if( iCalEvent.alarmUnits != DEFAULT_ALARMUNITS )
       alert( "TestFetchTodo(): Invalid Alarm Units" );
    if( iCalEvent.alarmLength != DEFAULT_ALARMLENGTH )
       alert( "TestFetchTodo(): Invalid Alarm Length" );
    if( iCalEvent.alarmEmailAddress != DEFAULT_EMAIL )
       alert( "TestFetchTodo(): Invalid Alarm Email Address" );
    if( iCalEvent.inviteEmailAddress != DEFAULT_EMAIL )
       alert( "TestFetchTodo(): Invalid Invite Email Address" );
    if( iCalEvent.recur != DEFAULT_RECUR )
       alert( "TestFetchTodo(): Invalid Recur Setting" );
    if( iCalEvent.recurInterval != DEFAULT_RECURINTERVAL )
       alert( "TestFetchTodo(): Invalid Recur Interval" );
    if( iCalEvent.recurUnits != DEFAULT_RECURUNITS )
       alert( "TestFetchTodo(): Invalid Recur Units" );
    if( iCalEvent.recurForever != DEFAULT_RECURFOREVER )
       alert( "TestFetchTodo(): Invalid Recur Forever" );

    //TODO: Check for start and end date

    return iCalEvent;
}

function TestUpdateTodo( iCalTodo )
{
    iCalTodo.title = DEFAULT_TITLE+"*NEW*";
    iCalTodo.description = DEFAULT_DESCRIPTION+"*NEW*";
    iCalTodo.location = DEFAULT_LOCATION+"*NEW*";
    iCalTodo.categories = DEFAULT_CATEGORY+"*NEW*";
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
       alert( "TestUpdateTodo(): Invalid Id" );
    if( iCalTodo.title != DEFAULT_TITLE+"*NEW*" )
       alert( "TestUpdateTodo(): Invalid Title" );
    if( iCalTodo.description != DEFAULT_DESCRIPTION+"*NEW*" )
       alert( "TestUpdateTodo(): Invalid Description" );
    if( iCalTodo.location != DEFAULT_LOCATION+"*NEW*" )
       alert( "TestUpdateTodo(): Invalid Location" );
    if( iCalTodo.categories != DEFAULT_CATEGORY+"*NEW*" )
       alert( "TestUpdateTodo(): Invalid Category" );
    if( iCalTodo.privateEvent != !DEFAULT_PRIVATE )
       alert( "TestUpdateTodo(): Invalid PrivateEvent Setting" );
    if( iCalTodo.allDay != !DEFAULT_ALLDAY )
       alert( "TestUpdateTodo(): Invalid AllDay Setting" );
    if( iCalTodo.alarm != !DEFAULT_ALARM )
       alert( "TestUpdateTodo(): Invalid Alarm Setting" );
    if( iCalTodo.recur != !DEFAULT_RECUR )
       alert( "TestUpdateTodo(): Invalid Recur Setting" );

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

function TestIcalString()
{

    var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
    var iCalEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);

    iCalEvent.id = 999999999;
    iCalEvent.title = DEFAULT_TITLE;
    iCalEvent.description = DEFAULT_DESCRIPTION;
    iCalEvent.location = DEFAULT_LOCATION;
    iCalEvent.categories = DEFAULT_CATEGORY;
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

    var sCalenderData = iCalEvent.getIcalString();
    var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
    var iCalParseEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
    //alert(sCalenderData);
    iCalParseEvent.parseIcalString( sCalenderData );
    //alert("2" + iCalParseEvent.description);

    if( iCalParseEvent.title != DEFAULT_TITLE )
       alert( "Invalid Title" );
    if( iCalParseEvent.description != DEFAULT_DESCRIPTION )
       alert( "Invalid Description" );
    if( iCalParseEvent.location != DEFAULT_LOCATION )
       alert( "Invalid Location" );
    if( iCalParseEvent.categories != DEFAULT_CATEGORY )
       alert( "Invalid Category" );
    if( iCalParseEvent.privateEvent != DEFAULT_PRIVATE )
       alert( "Invalid PrivateEvent Setting" );
    if( iCalParseEvent.allDay != DEFAULT_ALLDAY )
       alert( "Invalid AllDay Setting" );
    if( iCalParseEvent.alarm != DEFAULT_ALARM )
       alert( "Invalid Alarm Setting" );
    if( iCalParseEvent.alarmUnits != DEFAULT_ALARMUNITS )
       alert( "Invalid Alarm Units" );
    if( iCalParseEvent.alarmLength != DEFAULT_ALARMLENGTH )
       alert( "Invalid Alarm Length" );
    if( iCalParseEvent.alarmEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Alarm Email Address" );
    if( iCalParseEvent.inviteEmailAddress != DEFAULT_EMAIL )
       alert( "Invalid Invite Email Address" );
    if( iCalParseEvent.recur != DEFAULT_RECUR )
       alert( "Invalid Recur Setting" );
    if( iCalParseEvent.recurInterval != DEFAULT_RECURINTERVAL )
       alert( "Invalid Recur Interval" );
    if( iCalParseEvent.recurUnits != DEFAULT_RECURUNITS )
       alert( "Invalid Recur Units" );
    if( iCalParseEvent.recurForever != DEFAULT_RECURFOREVER )
       alert( "Invalid Recur Forever" );

    var iCalTodoComponent = Components.classes["@mozilla.org/icaltodo;1"].createInstance();
    
    var iCalTodo = iCalTodoComponent.QueryInterface(Components.interfaces.oeIICalTodo);

    iCalTodo.title = DEFAULT_TITLE;
    iCalTodo.description = DEFAULT_DESCRIPTION;
    iCalTodo.location = DEFAULT_LOCATION;
    iCalTodo.categories = DEFAULT_CATEGORY;
    iCalTodo.privateEvent = DEFAULT_PRIVATE;

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

    iCalTodo.id = 999999999;

    var icalstring = iCalTodo.getTodoIcalString();
    iCalTodo.parseTodoIcalString( icalstring );
    
    if( iCalTodo.id == null )
       alert( "TestAddTodo(): Invalid Id" );
    if( iCalTodo.title != DEFAULT_TITLE )
       alert( "TestAddTodo(): Invalid Title" );
    if( iCalTodo.description != DEFAULT_DESCRIPTION )
       alert( "TestAddTodo(): Invalid Description" );
    if( iCalTodo.location != DEFAULT_LOCATION )
       alert( "TestAddTodo(): Invalid Location" );
    if( iCalTodo.categories != DEFAULT_CATEGORY )
       alert( "TestAddTodo(): Invalid Category" );
    if( iCalTodo.privateEvent != DEFAULT_PRIVATE )
       alert( "TestAddTodo(): Invalid PrivateEvent Setting" );

    //TODO: Check for start and end date

    return true;
}
