/*
 * ***** BEGIN LICENSE BLOCK *****
 * The contents of this file are subject to the OEone End User License
 * Agreement; (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.oeone.com/EULA/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Calendar Code.
 * 
 * The Initial Developer of the Original Code is
 * Mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 1999-2002
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * ***** END LICENSE BLOCK *****
 */
 
var gAllowWindowOpen = true;
var gPendingEvents = new Array();

function openCalendar() 
{
   var wmediator = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);
      
   var calendarWindow = wmediator.getMostRecentWindow( "calendarMainWindow" );
   //the topWindow is always null, but it loads chrome://calendar/content/calendar.xul into the open window.

   if ( calendarWindow )
   {
      calendarWindow.focus();
   }
   else
      calendarWindow = window.open("chrome://calendar/content/calendar.xul", "calendar", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}


function getAlarmDialog( Event )
{
   var wmediator = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);

   var calendarAlarmWindow = wmediator.getMostRecentWindow( "calendarAlarmWindow" );
   //the topWindow is always null, but it loads chrome://calendar/content/calendar.xul into the open window.

   if ( calendarAlarmWindow )
      return calendarAlarmWindow;
   else
   {
      return false;
   }

}

function openCalendarAlarmWindow( Event )
{
   var args = new Object();

   args.calendarEvent = Event;
   
   calendarAlarmWindow = window.openDialog("chrome://calendar/content/alertDialog.xul", "caAlarmDialog", "chrome,extrachrome,resizable,scrollbars,status,toolbar,alwaysRaised", args);
   
   setTimeout( "resetAlarmDialog()", 2000 );
}

function resetAlarmDialog()
{
   gAllowWindowOpen = true;

   firePendingEvents();
}

function firePendingEvents()
{
   for( i = 0; i < gPendingEvents.length; i++ )
   {
      addEventToDialog( gPendingEvents[i] );
   }
}

function addEventToDialog( Event )
{
   var calendarAlarmWindow = getAlarmDialog( Event );
   if( calendarAlarmWindow )
   {
      dump( "\n\n!!!!!!!!!!!calendar alarm window, in iff" );  
      if( "createAlarmBox" in calendarAlarmWindow )
      {
        calendarAlarmWindow.onAlarmCall( Event );
      }
      else
      {
         if( !("pendingEvents" in calendarAlarmWindow) )
         {
             calendarAlarmWindow.pendingEvents = new Array();
         }
           
         dump( "\n ADDING PENDING EVENT TO DIALOG _______________________" );
         
         calendarAlarmWindow.pendingEvents.push( Event );
      }
    }
   else if( gAllowWindowOpen )
   {
      gAllowWindowOpen = false;
      
      if( getBoolPref(gCalendarWindow.calendarPreferences.calendarPref, "alarms.show", true ) )
      {
         calendarAlarmWindow = openCalendarAlarmWindow( Event );
      }
      else
      {
         gPendingEvents.push( Event );
      }
      
   }
   else
   {
      gPendingEvents.push( Event );
   }
}

