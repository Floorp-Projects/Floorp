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
 * The Original Code is Mozilla Calendar Code.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mike Potter <mikep@oeone.com>
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

function CalendarObject()
{
   this.path = "";
   this.name = "";
   this.remote = false;
   this.remotePath = "";
   this.active = false;
}

function calendarManager( CalendarWindow )
{
   this.CalendarWindow = CalendarWindow;

   this.calendars = new Array();

   this.getAllCalendars();

   for( var i = 0; i < this.calendars.length; i++ )
   {
      this.addCalendarToListBox( this.calendars[i] );
   }

   //if the calendars are being used, add them.
   this.addActiveCalendars();
}


calendarManager.prototype.addActiveCalendars = function()
{
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i].active == true )
      {
         this.addCalendar( this.calendars[i] );
      }                
   }
}


/*
** Launch the new calendar file dialog
*/
calendarManager.prototype.launchAddCalendarDialog = function( )
{
   // set up a bunch of args to pass to the dialog
   var ThisCalendarObject = new CalendarObject();
   ThisCalendarObject.name = "";
   ThisCalendarObject.path = "";
   ThisCalendarObject.remotePath = "";

   var args = new Object();
   args.mode = "new";

   var thisManager = this;

   var callback = function( ThisCalendarObject ) { thisManager.addServerDialogResponse( ThisCalendarObject ) };

   args.onOk =  callback;
   args.CalendarObject = ThisCalendarObject;

   // open the dialog modally
   openDialog("chrome://calendar/content/calendarServerDialog.xul", "caAddServer", "chrome,modal", args );
}


/*
** Called when OK is clicked in the new server dialog.
*/
calendarManager.prototype.addServerDialogResponse = function( CalendarObject )
{
   CalendarObject.active = true;

   var CurrentNumberOfServers = this.calendars.length;
   
   if( CalendarObject.path.indexOf( "http://" ) != -1 )
   {
      var profileComponent = Components.classes["@mozilla.org/profile/manager;1"].createInstance();
      
      var profileInternal = profileComponent.QueryInterface(Components.interfaces.nsIProfileInternal);
 
      var profileFile = profileInternal.getProfileDir(profileInternal.currentProfile);
      
      profileFile.append("RemoteCalendar"+CurrentNumberOfServers+".ics");

      CalendarObject.remotePath = CalendarObject.path;
      CalendarObject.path = profileFile.path;
      
      this.retrieveAndSaveRemoteCalendar( CalendarObject, onResponseAndRefresh );

      alert( "Remote Calendar Number "+CurrentNumberOfServers+" Added" );
   }
   else
   {
      alert( "Calendar Number "+CurrentNumberOfServers+" Added" );
   }
  
   //add the information to the preferences.
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "server"+CurrentNumberOfServers+".name", CalendarObject.name );
   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+CurrentNumberOfServers+".remote", true );
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "server"+CurrentNumberOfServers+".remotePath", CalendarObject.remotePath );
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "server"+CurrentNumberOfServers+".path", CalendarObject.path );
   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+CurrentNumberOfServers+".active", true );
   this.CalendarWindow.calendarPreferences.calendarPref.setIntPref(  "servers.count", (CurrentNumberOfServers+1) );
   
   //add this to the global calendars array
   this.calendars[ this.calendars.length ] = CalendarObject;

   this.addCalendarToListBox( CalendarObject );
}


/*
** Add the calendar so it is included in searches
*/
calendarManager.prototype.addCalendar = function( ThisCalendarObject )
{
   this.CalendarWindow.eventSource.gICalLib.addCalendar( ThisCalendarObject.path );

   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+this.getCalendarIndex( ThisCalendarObject )+".active", true );
}


/* 
** Remove the calendar, so it doesn't get included in searches 
*/
calendarManager.prototype.removeCalendar = function( ThisCalendarObject )
{
   this.CalendarWindow.eventSource.gICalLib.removeCalendar( ThisCalendarObject.path );

   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+this.getCalendarIndex( ThisCalendarObject )+".active", false );
}


/*
** Delete the calendar. Remove the file, it won't be used any longer.
*/
calendarManager.prototype.deleteCalendar = function( ThisCalendarObject )
{
   this.removeCalendar( ThisCalendarObject );
   
   //TODO: remove it from the array

   //TODO: remove it from the prefs

   //TODO: remove the file completely
}


calendarManager.prototype.getAllCalendars = function()
{
   this.calendars = new Array();

   var profileComponent = Components.classes["@mozilla.org/profile/manager;1"].createInstance();
      
   var profileInternal = profileComponent.QueryInterface(Components.interfaces.nsIProfileInternal);
 
   var profileFile = profileInternal.getProfileDir(profileInternal.currentProfile);
      
   profileFile.append("CalendarDataFile.ics");
                      
   //the root calendar is not stored in the prefs file.
   var thisCalendar = new CalendarObject;
   thisCalendar.name = "Default";
   thisCalendar.path = profileFile.path;
   var active;

   active = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "server0.active", true );

   thisCalendar.active = active;
   thisCalendar.remote = false;
   this.calendars[ this.calendars.length ] = thisCalendar;
   
   //go through the prefs file, calendars are stored in there.
   var NumberOfCalendars = getIntPref(this.CalendarWindow.calendarPreferences.calendarPref, "servers.count", 1 );
   
   var RefreshServers = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "servers.reloadonlaunch", false );
   
   //don't count the default server, so this starts at 1
   for( var i = 1; i < NumberOfCalendars; i++ )
   {
      thisCalendar = new CalendarObject();

      try { 
      
         thisCalendar.name = getCharPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+i+".name", "" );
         thisCalendar.path = getCharPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+i+".path", "" );
         thisCalendar.active = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+i+".active", false );
         thisCalendar.remote = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+i+".remote", false );
         thisCalendar.remotePath = getCharPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+i+".remotePath", "" );
         
         if( RefreshServers == true 
            && thisCalendar.remote == true )
            this.retrieveAndSaveRemoteCalendar( thisCalendar );
         
         this.calendars[ this.calendars.length ] = thisCalendar;
      }
      catch ( e )
      {
         dump( "error: could not get calendar information from preferences\n"+e );
      }
   }
}


calendarManager.prototype.addCalendarToListBox = function( ThisCalendarObject )
{
   var calendarListItem = document.createElement( "listitem" );
   calendarListItem.setAttribute( "id", "calendar-list-item" );
   calendarListItem.setAttribute( "onclick", "removeCalendar( event );" );
   calendarListItem.calendarObject = ThisCalendarObject;
   calendarListItem.setAttribute( "label", ThisCalendarObject.name );
   calendarListItem.setAttribute( "flex", "1" );
   
   calendarListItem.setAttribute( "calendarPath", ThisCalendarObject.path );

   calendarListItem.setAttribute( "type", "checkbox" );
   calendarListItem.setAttribute( "checked", ThisCalendarObject.active );
   
   document.getElementById( "list-calendars-listbox" ).appendChild( calendarListItem );
}


calendarManager.prototype.getCalendarIndex = function( ThisCalendarObject )
{
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i] === ThisCalendarObject )
      {
         return( i );
      }
   }
   return( false );
}

var xmlhttprequest;
var request;
var calendarToGet = null;
var refresh = false;

calendarManager.prototype.retrieveAndSaveRemoteCalendar = function( ThisCalendarObject, onResponse )
{
   calendarToGet = ThisCalendarObject;
   // make a request
   xmlhttprequest = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
   request =  xmlhttprequest.createInstance( Components.interfaces.nsIXMLHttpRequest );
    
   request.addEventListener( "load", onResponse, false );
   request.addEventListener( "error", onError, false );
   
   request.open( "GET", ThisCalendarObject.remotePath, true );    // true means async
   
   request.send( null );
}


calendarManager.prototype.refreshAllRemoteCalendars = function()
{
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i].remote == true )
      {
         refresh = true;
         
         this.retrieveAndSaveRemoteCalendar( this.calendars[i], onResponseAndRefresh );
      }
   }
}


function onResponseAndRefresh( )
{
   //save the stream to a file.
   saveDataToFile( calendarToGet.path, request.responseText, "UTF-8" );
   
   gCalendarWindow.calendarManager.removeCalendar( calendarToGet );

   gCalendarWindow.calendarManager.addCalendar( calendarToGet );

   //refresh the views
   var eventTable = gEventSource.getCurrentEvents();

   refreshEventTree( eventTable );

   gCalendarWindow.currentView.refreshEvents();
}

function onError( )
{
   alert( "error: Could not load remote calendar" );
}

function removeCalendar( event )
{
   dump( "\nRemoveCalendar in calendarManager.js: button is "+event.button );
   if (event.button != 0) 
   {
      return;
   }
      
   if( event.currentTarget.checked )
      gCalendarWindow.calendarManager.addCalendar( event.currentTarget.calendarObject );
   else
      gCalendarWindow.calendarManager.removeCalendar( event.currentTarget.calendarObject );

   //refresh the views
   var eventTable = gEventSource.getCurrentEvents();

   refreshEventTree( eventTable );

   gCalendarWindow.currentView.refreshEvents();
}
