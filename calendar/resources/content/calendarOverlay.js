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


function openCalendar() 
{
   var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
   
   var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
   
   var topWindow = windowManagerInterface.getMostRecentWindow( "calendar" );
   
   //the topWindow is always null, but it loads chrome://calendar/content/calendar.xul into the open window.

   if ( topWindow )
   {
      topWindow.focus();
   }
   else
      window.open("chrome://calendar/content/calendar.xul", "calendar", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
}


function getAlarmDialog( Event )
{
   var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
 
   var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);

   var topWindow = windowManagerInterface.getMostRecentWindow( "caAlarmDialog" );
   
   if ( topWindow )
      return topWindow;
   else
   {
      var args = new Object();

      args.calendarEvent = Event;
   
      window.open("chrome://calendar/content/ca-event-alert-dialog.xul", "caAlarmDialog", "chrome,extrachrome,resizable,scrollbars,status,toolbar,alwaysRaised", args);

      return false;
   }

}

function addEventToDialog( Event )
{
   var alarmWindow = getAlarmDialog( Event );
   if( alarmWindow )
    {
        if( "createAlarmBox" in alarmWindow )
        {
           alarmWindow.onAlarmCall( Event );
        }
        else
        {
            if( !("pendingEvents" in alarmWindow) )
            {
                alarmWindow.pendingEvents = new Array();
            }
            
            //dump( "\n ADDING PENDING EVENT TO DIALOG _______________________" );
            
            alarmWindow.pendingEvents.push( Event );
        }
        
    }
}

