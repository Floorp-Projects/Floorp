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
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
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

var gAllEvents = new Array();
var CreateAlarmBox = true;
var kungFooDeathGripOnEventBoxes = new Array();
var gICalLib;

function onLoad()
{
   var calendarEventService = opener.gEventSource;
   
   gICalLib = calendarEventService.getICalLib();

   var args = window.arguments[0];
   
   if( args.calendarEvent )
   {
      onAlarmCall( args.calendarEvent );
   }
   
   if( "pendingEvents" in window )
   {
        //dump( "\n GETTING PENDING ___________________" );
        for( var i in window.pendingEvents )
        {
           gAllEvents[ gAllEvents.length ] = window.pendingEvents[i]; 
        }
                
        buildEventBoxes();
   }
   
   doSetOKCancel( onOkButton, 0 );
}

function buildEventBoxes()
{
   //remove all the old event boxes.
   var EventContainer = document.getElementById( "event-container-rows" );
   var tooManyDescValue ;

   if( EventContainer )
   {
      var NumberOfChildElements = EventContainer.childNodes.length;
      for( i = 0; i < NumberOfChildElements; i++ )   
      {
         EventContainer.removeChild( EventContainer.lastChild );
      }
      
      //start at length - 6 or 0 if that is < 0
   
      var Start = gAllEvents.length - 6;
      if( Start < 0 )
         Start = 0;
   
      //build all event boxes again.
      for( var i = Start; i < gAllEvents.length; i++ )
      {
         createAlarmBox( gAllEvents[i] );   
      }
      
      //reset the text
      if( gAllEvents.length > 6 )
      {
         var TooManyDesc = document.getElementById( "too-many-alarms-description" );
         TooManyDesc.removeAttribute( "collapsed" );         
	 tooManyDescValue = gCalendarBundle.getFormattedString("TooManyAlarmsMessage", [gAllEvents.length]);
         TooManyDesc.setAttribute( "value", tooManyDescValue );
      }
      else
      {
         var TooManyDesc = document.getElementById( "too-many-alarms-description" );
         TooManyDesc.setAttribute( "collapsed", "true" );
      }

   }
   sizeToContent();
}

function onAlarmCall( Event )
{
   var AddToArray = true;
   //check and make sure that the event is not already in the array
   for( var i = 0; i < gAllEvents.length; i++ )
   {
      if( gAllEvents[i].id == Event.id )
         AddToArray = false;   
   }
   if( AddToArray )
      gAllEvents[ gAllEvents.length ] = Event;

   buildEventBoxes();
}

function createAlarmBox( Event )
{
   var OuterBox = document.getElementsByAttribute( "name", "sample-row" )[0].cloneNode( true );
   OuterBox.removeAttribute( "name" );
   OuterBox.setAttribute( "id", Event.id );
   OuterBox.setAttribute( "eventbox", "true" );
   OuterBox.removeAttribute( "collapsed" );

   OuterBox.getElementsByAttribute( "name", "AcknowledgeButton" )[0].event = Event;
   
   OuterBox.getElementsByAttribute( "name", "AcknowledgeButton" )[0].setAttribute( "onclick", "acknowledgeAlarm( this.event );removeAlarmBox( this.event );" ); 
   
   OuterBox.getElementsByAttribute( "name", "EditEvent" )[0].event = Event;
   
   OuterBox.getElementsByAttribute( "name", "EditEvent" )[0].setAttribute( "onclick", "launchEditEvent( this.event );" ); 
   
   kungFooDeathGripOnEventBoxes.push( OuterBox.getElementsByAttribute( "name", "AcknowledgeButton" )[0] );
   
   OuterBox.getElementsByAttribute( "name", "SnoozeButton" )[0].event = Event;
   
   OuterBox.getElementsByAttribute( "name", "SnoozeButton" )[0].setAttribute( "onclick", "snoozeAlarm( this.event );removeAlarmBox( this.event );" ); 
   
   var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService);
   
    OuterBox.getElementsByAttribute( "name", "alarm-length-field" )[0].value = getIntPref(prefService.getBranch("calendar."), "alarms.defaultsnoozelength", gCalendarBundle.getString("defaultSnoozeAlarmLength" ) );
      
   kungFooDeathGripOnEventBoxes.push( OuterBox.getElementsByAttribute( "name", "SnoozeButton" )[0] );
   
   /*
   ** The first part of the box, the title and description
   */
   var EventTitle = document.createTextNode( Event.title );
   OuterBox.getElementsByAttribute( "name", "Title" )[0].appendChild( EventTitle );
   
   var EventDescription = document.createTextNode( Event.description );
   OuterBox.getElementsByAttribute( "name", "Description" )[0].appendChild( EventDescription );

   if( !Event.recur )
       var displayDate = new Date( Event.start.getTime() );
   else
       var displayDate = new Date( Event.displayDate );

   var EventDisplayDate = document.createTextNode( getFormatedDate( displayDate ) );
   OuterBox.getElementsByAttribute( "name", "StartDate" )[0].appendChild( EventDisplayDate );

   var EventDisplayTime = document.createTextNode( getFormatedTime( displayDate ) );
   OuterBox.getElementsByAttribute( "name", "StartTime" )[0].appendChild( EventDisplayTime );

   /*
   ** 3rd part of the row: the number of times that alarm went off (sometimes hidden)
   */
   OuterBox.getElementsByAttribute( "name", "NumberOfTimes" )[0].setAttribute( "id", "description-"+Event.id );
   
   //document.getElementById( "event-container-rows" ).insertBefore( OuterBox, document.getElementById( "event-container-rows" ).childNodes[1] );
   document.getElementById( "event-container-rows" ).appendChild( OuterBox );
}

function removeAlarmBox( Event )
{
   
   //get the box for the event
   var EventAlarmBox = document.getElementById( Event.id );
   
   if( EventAlarmBox )
   {
      //remove the box from the body
      EventAlarmBox.parentNode.removeChild( EventAlarmBox );
   }
   
   //if there's no more events left, close the dialog
   EventAlarmBoxes = document.getElementsByAttribute( "eventbox", "true" );
      
   if( EventAlarmBoxes.item(0) )
   {
      //there are still boxes left.
      return( false );
   }
   else
   {
      //close the dialog
      self.close();
   }
}

function getArrayId( Event )
{
   for( i = 0; i < gAllEvents.length; i++ )
   {
      if( gAllEvents[i].id == Event.id )
         return( i );
   }
   
   return( false );

}

function onOkButton( )
{
   //this would acknowledge all the alarms
   for( i = 0; i < gAllEvents.length; i++ )
   {
      gAllEvents[i].lastAlarmAck = new Date();
   }

   for( i = 0; i < gAllEvents.length; i++ )
   {
      gICalLib.modifyEvent( gAllEvents[i] );
   }

   return( true );
}

function onCancelButton()
{
   //just close the dialog
   return( true );

}

function acknowledgeAlarm( Event )
{
   Event.lastAlarmAck = new Date();

   var calendarEventService = opener.gEventSource;
   
   gICalLib = calendarEventService.getICalLib();

   gICalLib.modifyEvent( Event );

   var Id = getArrayId( Event )

   gAllEvents.splice( Id, 1 );

   buildEventBoxes();
}

function launchEditEvent( Event )
{
   // set up a bunch of args to pass to the dialog
   
   var args = new Object();
   args.mode = "edit";
   args.onOk = modifyEventDialogResponse;           
   args.calendarEvent = Event;

   // open the dialog modally
   window.setCursor( "wait" );
    if( Event.type == Event.XPICAL_VEVENT_COMPONENT )
        opener.openDialog("chrome://calendar/content/eventDialog.xul", "caEditEvent", "chrome,modal", args );
    else
        opener.openDialog("chrome://calendar/content/toDoDialog.xul", "caEditEvent", "chrome,modal", args );
}

function modifyEventDialogResponse( calendarEvent )
{
   gICalLib.modifyEvent( calendarEvent );
}

/**
*   Read snooze length and units from dialog, run event.setSnooze
*   TODO: snoozelength only accepts decimal point (not comma)
*/
function snoozeAlarm( Event )
{
   var OuterBox = document.getElementById( Event.id );
   
   var snoozeLength= OuterBox.getElementsByAttribute( "name", "alarm-length-field" )[0].value;
   snoozeLength= parseFloat( snoozeLength );

   var SnoozeUnits = document.getElementsByAttribute( "name", "alarm-length-units" )[0].value;

   var Now = ( new Date() ).getTime();

   var TimeToNextAlarm;
   
   switch (SnoozeUnits )
   {
      case "minutes":
         TimeToNextAlarm = parseInt( kDate_MillisecondsInMinute * snoozeLength );
         break;

      case "hours":
         TimeToNextAlarm = parseInt( kDate_MillisecondsInHour * snoozeLength );
         break;

      case "days":
         TimeToNextAlarm = parseInt( kDate_MillisecondsInDay * snoozeLength );
         break;
   }
   
   var DateObjOfNextAlarm = new Date( Now + TimeToNextAlarm);
   
   Event.setSnoozeTime( DateObjOfNextAlarm );
   
   var calendarEventService = opener.gEventSource;
   
   gICalLib = calendarEventService.getICalLib();
   gICalLib.modifyEvent( Event );

   var Id = getArrayId( Event )

   gAllEvents.splice( Id, 1 );

   buildEventBoxes();
}

function getFormatedDate( date )
{
   return( opener.gCalendarWindow.dateFormater.getFormatedDate( date ) );
}


/**
*   Take a Date object and return a displayable time string i.e.: 12:30 PM
*/

function getFormatedTime( date )
{
   var timeString = opener.gCalendarWindow.dateFormater.getFormatedTime( date );
   
   return timeString;
}


function getIntPref (prefObj, prefName, defaultValue)
{
    try
    {
        return prefObj.getIntPref (prefName);
    }
    catch (e)
    {
       prefObj.setIntPref( prefName, defaultValue );  
       return defaultValue;
    }
}
