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


var gCalendarEvent = null;

function onLoad()
{
   gCalendarEvent = window.arguments[0];
   createAlarmText( gCalendarEvent );
   setupOkCancelButtons( onOkButton, 0 );
   window.resizeTo( 455, 150 );
}

function createAlarmText( )
{
   var Text = opener.penapplication.username+" has an event titled "+gCalendarEvent.title;
   var TextBox = document.getElementById( "event-message-box" );
   var HtmlInBox = document.createElement( "html" );
   var TextInBox = document.createTextNode( Text );
   HtmlInBox.appendChild( TextInBox );
   TextBox.appendChild( HtmlInBox );

   var Text = " at "+gCalendarEvent.start;
   var TextBox = document.getElementById( "event-message-time-box" );
   var HtmlInBox = document.createElement( "html" );
   var TextInBox = document.createTextNode( Text );
   HtmlInBox.appendChild( TextInBox );
   TextBox.appendChild( HtmlInBox );
}

function onOkButton( )
{
   var RemindCheckbox = document.getElementById( "remind-again-checkbox" );

   if ( RemindCheckbox.getAttribute( "checked" ) == "true" ) 
   {
      snoozeAlarm();
   }
   else
   {
      acknowledgeAlarm();
   }
   return( true );
}

function acknowledgeAlarm( )
{
   gCalendarEvent.alarmWentOff = true;

   gCalendarEvent.snoozeTime = false;
            
   //opener.gEventSource.modifyEvent( gCalendarEvent );

   pendialog.getRoot().Root.gCalendarEventDataSource.respondAcknowledgeAlarm( gCalendarEvent );
   
   return( true );
}

function snoozeAlarm( )
{
   gCalendarEvent.alarmWentOff = true;

   var nowMS = new Date().getTime();

   var minutesToSnooze = document.getElementById( "dialog-alarm-minutes" ).getAttribute( "value" );
   
   var snoozeTime = nowMS + ( 1000 * minutesToSnooze * 60 );
   
   snoozeTime = parseInt( snoozeTime );

   //alert( snoozeTime );

   gCalendarEvent.snoozeTime = snoozeTime;
   
   gCalendarEvent.setUpAlarmTimer( this );

   //opener.gEventSource.modifyEvent( gCalendarEvent );

   pendialog.getRoot().Root.gCalendarEventDataSource.respondAcknowledgeAlarm( gCalendarEvent );

   return( true );
}
