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

function Test()
{
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
    
    this.iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
    
    this.iCalLib.SetServer( "/home/mostafah/calendar" );
    
    this.iCalLib.Test();

}

function TestAddEvent()
{
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
    
    this.iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
    
    var iCalLibEvent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
    
    this.iCalLibEvent = iCalLibEvent.QueryInterface(Components.interfaces.oeIICalEvent);

    this.iCalLib.SetServer( "/home/mostafah/calendar" );
    
    iCalLibEvent.Title = "Lunch Time";
    iCalLibEvent.Description = "Will be out for one hour";
    iCalLibEvent.Location = "Restaurant";
    iCalLibEvent.Category = "Personal";
    iCalLibEvent.PrivateEvent = false;
    iCalLibEvent.AllDay = true;
    iCalLibEvent.AlarmLength = 55;
    iCalLibEvent.Alarm = true;
//    iCalLibEvent.AlarmWentOff = false;
    iCalLibEvent.AlarmEmailAddress = "mostafah@oeone.com";
    iCalLibEvent.InviteEmailAddress = "mostafah@oeone.com";
    iCalLibEvent.SnoozeTime = "5";
    iCalLibEvent.RecurType = 3;
    iCalLibEvent.RecurInterval = 7;
    iCalLibEvent.RepeatUnits = "days";
    iCalLibEvent.RepeatForever = true;
    iCalLibEvent.SetStartDate( 2001, 9, 22, 12, 24 );
    iCalLibEvent.SetEndDate( 2001, 9, 22, 13, 24 );
    iCalLibEvent.SetRecurInfo( 1, 1, 2002, 9, 21 );
//    iCalLibEvent.SetAlarm( 2001, 9, 21, 12, 26 );
    
    var id = this.iCalLib.AddEvent( iCalLibEvent );
    
//    alert( "Id:"+id );
//    alert( "Title:"+iCalLibEvent.Title );
//    alert( "Description:"+iCalLibEvent.Description );
//    alert( "Location:"+iCalLibEvent.Location );
//    alert( "Category:"+iCalLibEvent.Category );
//    alert( "IsPrivate:"+iCalLibEvent.PrivateEvent );
//    alert( "AllDay:"+iCalLibEvent.AllDay );
//    alert( "Alarm:"+iCalLibEvent.Alarm );
//    alert( "AlarmWentOff:"+iCalLibEvent.AlarmWentOff );
//    alert( "AlarmLength:"+iCalLibEvent.AlarmLength );
//    alert( "AlarmEmailAddress:"+iCalLibEvent.AlarmEmailAddress );
//    alert( "InviteEmailAddress:"+iCalLibEvent.InviteEmailAddress );
//    alert( "SnoozeTime:"+iCalLibEvent.SnoozeTime );
//    alert( "RecurType:"+iCalLibEvent.RecurType );
//    alert( "RecurInterval:"+iCalLibEvent.RecurInterval );
//    alert( "RepeatUnits:"+iCalLibEvent.RepeatUnits );
//    alert( "RepeatForever:"+iCalLibEvent.RepeatForever );
//    alert( "StartDate:"+iCalLibEvent.GetStartDate() );
//    alert( "EndDate:"+iCalLibEvent.GetEndDate() );
//    alert( "EndDate:"+iCalLibEvent.GetRecurEndDate() );
//    var result = iCalLibEvent.GetNextRecurrence( 2001, 8, 28 );
//    alert( result );
    return id;
}

function TestFetchEvent( id )
{
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
    
    this.iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
    
    this.iCalLib.SetServer( "/home/mostafah/calendar" );
    var iCalLibEventFetched = iCalLib.FetchEvent( id );
//    alert( "Title:"+iCalLibEventFetched.Title );
//    alert( "Description:"+iCalLibEventFetched.Description );
//    alert( "Location:"+iCalLibEventFetched.Location );
//    alert( "Category:"+iCalLibEventFetched.Category );
//    alert( "IsPrivate:"+iCalLibEventFetched.PrivateEvent );
//    alert( "AllDay:"+iCalLibEventFetched.AllDay );
//    alert( "Alarm:"+iCalLibEventFetched.Alarm );
//    alert( "AlarmWentOff:"+iCalLibEventFetched.AlarmWentOff );
//    alert( "AlarmLength:"+iCalLibEventFetched.AlarmLength );
//    alert( "AlarmEmailAddress:"+iCalLibEventFetched.AlarmEmailAddress );
//    alert( "InviteEmailAddress:"+iCalLibEventFetched.InviteEmailAddress );
//    alert( "SnoozeTime:"+iCalLibEventFetched.SnoozeTime );
//    alert( "RecurType:"+iCalLibEventFetched.RecurType );
//    alert( "RecurInterval:"+iCalLibEventFetched.RecurInterval );
//    alert( "RepeatUnits:"+iCalLibEventFetched.RepeatUnits );
//    alert( "RepeatForever:"+iCalLibEventFetched.RepeatForever );
//    alert( "StartDate:"+iCalLibEventFetched.GetStartDate() );
//    alert( "EndDate:"+iCalLibEventFetched.GetEndDate() );
}

function TestUpdateEvent( iCalLibEvent )
{
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
    
    this.iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
    
    this.iCalLib.SetServer( "/home/mostafah/calendar" );
    
    iCalLibEvent.Title = "Lunch & Learn";
    iCalLibEvent.Location = "Conference Room";
    
    var id = this.iCalLib.UpdateEvent( iCalLibEvent );
    
//    alert( "Id:"+id );
//    alert( "Title:"+iCalLibEvent.Title );
//    alert( "Description:"+iCalLibEvent.Description );
//    alert( "Location:"+iCalLibEvent.Location );
//    alert( "Category:"+iCalLibEvent.Category );
//    alert( "IsPrivate:"+iCalLibEvent.PrivateEvent );
//    alert( "AllDay:"+iCalLibEvent.AllDay );
//    alert( "Alarm:"+iCalLibEvent.Alarm );
//    alert( "AlarmWentOff:"+iCalLibEvent.AlarmWentOff );
//    alert( "AlarmLength:"+iCalLibEvent.AlarmLength );
//    alert( "AlarmEmailAddress:"+iCalLibEvent.AlarmEmailAddress );
//    alert( "InviteEmailAddress:"+iCalLibEvent.InviteEmailAddress );
//    alert( "SnoozeTime:"+iCalLibEvent.SnoozeTime );
//    alert( "RecurType:"+iCalLibEvent.RecurType );
//    alert( "RecurInterval:"+iCalLibEvent.RecurInterval );
//    alert( "RepeatUnits:"+iCalLibEvent.RepeatUnits );
//    alert( "RepeatForever:"+iCalLibEvent.RepeatForever );
//    alert( "StartDate:"+iCalLibEvent.GetStartDate() );
//    alert( "EndDate:"+iCalLibEvent.GetEndDate() );
    return id;
}

function TestDeleteEvent( id )
{
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
    
    this.iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
    
    this.iCalLib.SetServer( "/home/mostafah/calendar" );
    
    iCalLib.DeleteEvent( id );
}

function TestSearchEvent()
{
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    var iCalLibComponent = Components.classes["@mozilla.org/ical;1"].createInstance();
    
    this.iCalLib = iCalLibComponent.QueryInterface(Components.interfaces.oeIICal);
    
    this.iCalLib.SetServer( "/home/mostafah/calendar" );
    
    var result = this.iCalLib.SearchEvent( 2000,01,01,00,00,2002,01,01,00,00 );
    result = this.iCalLib.SearchForEvent( "SELECT * FROM VEVENT WHERE CATEGORIES = 'Personal'" );
    result = this.iCalLib.SearchAlarm( 2001,9,22,11,30 );
    alert( "Result : " + result );
}

function TestAll()
{
    var id = TestAddEvent();
    var iCalLibEventFetched = TestFetchEvent( id );
    id = TestUpdateEvent( iCalLibEvent );
    TestSearchEvent();
    TestDeleteEvent( id );
}