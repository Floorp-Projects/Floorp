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

function calendarManager( CalendarWindow )
{
   this.parent = CalendarWindow;

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
   for( var i = 1; i < this.calendars.length; i++ )
   {
      if( this.calendars[i].active === true )
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
   var CalendarObject = new Object();
   CalendarObject.name = "";
   CalendarObject.path = "";

   var args = new Object();
   args.mode = "new";

   var thisManager = this;

   var callback = function( CalendarObject ) { thisManager.addServerDialogResponse( CalendarObject ) };

   args.onOk =  callback;
   args.CalendarObject = CalendarObject;

   // open the dialog modally
   openDialog("chrome://calendar/content/calendarServerDialog.xul", "caAddServer", "chrome,modal", args );
}


/*
** Called when OK is clicked in the new server dialog.
*/
calendarManager.prototype.addServerDialogResponse = function( CalendarObject )
{
   var CurrentNumberOfServers = this.calendars.length;
   
   //add the information to the preferences.
   this.parent.calendarPreferences.calendarPref.setCharPref( "server"+CurrentNumberOfServers+".name", CalendarObject.name );
   this.parent.calendarPreferences.calendarPref.setCharPref( "server"+CurrentNumberOfServers+".path", CalendarObject.path );
   this.parent.calendarPreferences.calendarPref.setBoolPref( "server"+CurrentNumberOfServers+".active", true );
   this.parent.calendarPreferences.calendarPref.setIntPref(  "servers.count", CurrentNumberOfServers );
   
   //add this to the global calendars array
   this.calendars[ this.calendars.length ] = CalendarObject;
}


/*
** Add the calendar so it is included in searches
*/
calendarManager.prototype.addCalendar = function( CalendarObject )
{
   this.parent.eventSource.gICalLib.addCalendar( CalendarObject.path );

   this.parent.calendarPreferences.calendarPref.setBoolPref( "server"+this.getCalendarIndex( CalendarObject )+".active", true );
}


/* 
** Remove the calendar, so it doesn't get included in searches 
*/
calendarManager.prototype.removeCalendar = function( CalendarObject )
{
   this.parent.eventSource.gICalLib.removeCalendar( CalendarObject.path );

   this.parent.calendarPreferences.calendarPref.setBoolPref( "server"+this.getCalendarIndex( CalendarObject )+".active", false );
}


/*
** Delete the calendar. Remove the file, it won't be used any longer.
*/
calendarManager.prototype.deleteCalendar = function( CalendarObject )
{
   this.removeCalendar( CalendarObject );
   
   //TODO: remove it from the array

   //TODO: remove it from the prefs

   //TODO: remove the file completely
}


calendarManager.prototype.getAllCalendars = function()
{
   var profileComponent = Components.classes["@mozilla.org/profile/manager;1"].createInstance();
      
   var profileInternal = profileComponent.QueryInterface(Components.interfaces.nsIProfileInternal);
 
   var profileFile = profileInternal.getProfileDir(profileInternal.currentProfile);
      
   profileFile.append("CalendarDataFile.ics");
                      
   //the root calendar is not stored in the prefs file.
   var thisCalendar = new Object;
   thisCalendar.name = "Default";
   thisCalendar.path = profileFile.path;
   thisCalendar.active = true;

   this.calendars[ this.calendars.length ] = thisCalendar;
   
   //go through the prefs file, calendars are stored in there.
   var NumberOfCalendars = this.parent.calendarPreferences.arrayOfPrefs.numberofservers;

   //don't count the default server, so this starts at 1
   for( var i = 1; i < NumberOfCalendars; i++ )
   {
      thisCalendar = new Object();

      try { 
      
         thisCalendar.name = this.parent.calendarPreferences.calendarPref.getCharPref( "server"+i+".name" );

         thisCalendar.path = this.parent.calendarPreferences.calendarPref.getCharPref( "server"+i+".path" );

         thisCalendar.active = this.parent.calendarPreferences.calendarPref.getBoolPref( "server"+i+".active" );

         this.calendars[ this.calendars.length ] = thisCalendar;
      }
      catch ( e )
      {

      }
   }
}


calendarManager.prototype.addCalendarToListBox = function( CalendarObject )
{
   var calendarListItem = document.createElement( "listitem" );
   calendarListItem.setAttribute( "id", "calendar-list-item" );
   calendarListItem.setAttribute( "onclick", "removeCalendar( event );" );
   calendarListItem.calendarObject = CalendarObject;

   var ListCell = document.createElement( "listcell" );
   var CheckBox = document.createElement( "checkbox" );
   CheckBox.setAttribute( "checked", CalendarObject.active );
   ListCell.appendChild( CheckBox );
   calendarListItem.appendChild( ListCell );

   ListCell = document.createElement( "listcell" );
   ListCell.setAttribute( "flex", "1" );
   ListCell.setAttribute( "label", CalendarObject.name );
   calendarListItem.appendChild( ListCell );

   calendarListItem.setAttribute( "calendarPath", CalendarObject.path );

   document.getElementById( "list-calendars-listbox" ).appendChild( calendarListItem );
}


calendarManager.prototype.getCalendarIndex = function( CalendarObject )
{
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i] === CalendarObject )
      {
         return( i );
      }
   }
   return( false );
}

function removeCalendar( event )
{
   var ThisCheckbox = event.currentTarget.getElementsByTagName( "checkbox" );

   ThisCheckbox[0].checked = !ThisCheckbox[0].checked;

   if( ThisCheckbox[0].checked == true )
      gCalendarWindow.calendarManager.addCalendar( event.currentTarget.calendarObject );
   else
      gCalendarWindow.calendarManager.removeCalendar( event.currentTarget.calendarObject );

   //refresh the views
   var eventTable = gEventSource.getCurrentEvents();

   refreshEventTree( eventTable );

   gCalendarWindow.currentView.refreshEvents();
}
