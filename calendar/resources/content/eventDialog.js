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
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Chris Charabaruk <ccharabaruk@meldstar.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
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



/***** calendar/calendarEventDialog.js
* AUTHOR
*   Garth Smedley
* REQUIRED INCLUDES 
*   <script type="application/x-javascript" src="chrome://calendar/content/dateUtils.js"/>
*   
* NOTES
*   Code for the calendar's new/edit event dialog.
*
*  Invoke this dialog to create a new event as follows:

   var args = new Object();
   args.mode = "new";               // "new" or "edit"
   args.onOk = <function>;          // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // newly creatd calendar event to be editted
    
   calendar.openDialog("caNewEvent", "chrome://calendar/content/eventDialog.xul", true, args );
*
*  Invoke this dialog to edit an existing event as follows:
*
   var args = new Object();
   args.mode = "edit";                    // "new" or "edit"
   args.onOk = <function>;                // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // javascript object containin the event to be editted

* When the user clicks OK the onOk function will be called with a calendar event object.
*
*  
* IMPLEMENTATION NOTES 
**********
*/


/*-----------------------------------------------------------------
*   W I N D O W      V A R I A B L E S
*/


var gEvent;          // event being edited
var gOnOkFunction;   // function to be called when user clicks OK

var gTimeDifference = 3600000;  //when editing an event, we change the end time if the start time is changing. This is the difference for the event time.
var gDateDifference = 3600000;  //this is the difference for the dates, not the times.

var gDefaultAlarmLength = 15; //number of minutes to default the alarm to
var gCategoryManager;

var gMode = ''; //what mode are we in? new or edit...

const kRepeatDay_0 = 1; //Sunday
const kRepeatDay_1 = 2; //Monday
const kRepeatDay_2 = 4; //Tuesday
const kRepeatDay_3 = 8; //Wednesday
const kRepeatDay_4 = 16;//Thursday
const kRepeatDay_5 = 32;//Friday
const kRepeatDay_6 = 64;//Saturday


/*-----------------------------------------------------------------
*   W I N D O W      F U N C T I O N S
*/

/**
*   Called when the dialog is loaded.
*/

function loadCalendarEventDialog()
{
   // Get arguments, see description at top of file
   
   var args = window.arguments[0];
   
   gMode = args.mode;
   
   gOnOkFunction = args.onOk;
   gEvent = args.calendarEvent;
   
   // mode is "new or "edit" - show proper header
   var titleDataItem = null;

   if( "new" == args.mode )
   {
      titleDataItem = document.getElementById( "data-event-title-new" );
   }
   else
   {
      titleDataItem = document.getElementById( "data-event-title-edit" );
   }
   
   var titleString = titleDataItem.getAttribute( "value" );
   document.getElementById("calendar-new-eventwindow").setAttribute("title", titleString);
   
   // fill in fields from the event
   var startDate = new Date( gEvent.start.getTime() );
   document.getElementById( "start-date-picker" ).value = startDate;
   
   var endDate = new Date( gEvent.end.getTime() );
   document.getElementById( "end-date-picker" ).value = endDate;
   
   setTimeFieldValue( "start-time-text", startDate );
   setTimeFieldValue( "end-time-text", endDate );
   
   gTimeDifference = gEvent.end.getTime() - gEvent.start.getTime(); //the time difference in ms
   gDateDifference = gTimeDifference; //the time difference in ms
   
   var today = new Date();

   if ( gEvent.recurForever ) 
   {
      gEvent.recurEnd.setTime( endDate );
   }

   var recurEndDate = new Date( gEvent.recurEnd.getTime() );
   
   //do the stuff for exceptions
   var ArrayOfExceptions = gEvent.getExceptions();

   while( ArrayOfExceptions.hasMoreElements() )
   {
      ExceptionTime = ArrayOfExceptions.getNext().QueryInterface(Components.interfaces.nsISupportsPRTime).data;
      
      var ExceptionDate = new Date( ExceptionTime );

      addException( ExceptionDate );
   }

   //file attachments;
   for( var i = 0; i < gEvent.attachmentsArray.Count(); i++ )
   {
      var thisAttachment = gEvent.attachmentsArray.QueryElementAt( i, Components.interfaces.nsIMsgAttachment );
      
      addAttachment( thisAttachment );
   }

   document.getElementById( "exceptions-date-picker" ).value = startDate;
      
   setFieldValue( "title-field", gEvent.title  );
   setFieldValue( "description-field", gEvent.description );
   setFieldValue( "location-field", gEvent.location );
   setFieldValue( "uri-field", gEvent.url );
   
   switch( gEvent.status )
   {
      case gEvent.ICAL_STATUS_TENTATIVE:
         setFieldValue( "status-field", "ICAL_STATUS_TENTATIVE" );
      break;
      case gEvent.ICAL_STATUS_CONFIRMED:
         setFieldValue( "status-field", "ICAL_STATUS_CONFIRMED" );
      break;
      case gEvent.ICAL_STATUS_CANCELLED:
         setFieldValue( "status-field", "ICAL_STATUS_CANCELLED" );
      break;
   }
   
   setFieldValue( "all-day-event-checkbox", gEvent.allDay, "checked" );
   setFieldValue( "private-checkbox", gEvent.privateEvent, "checked" );
   
   if( gEvent.alarm === false && gEvent.alarmLength == 0 )
   {
      gEvent.alarmLength = gDefaultAlarmLength;
   }
   
   setFieldValue( "alarm-checkbox", gEvent.alarm, "checked" );
   setFieldValue( "alarm-length-field", gEvent.alarmLength );
   setFieldValue( "alarm-length-units", gEvent.alarmUnits );

   if( gEvent.alarmEmailAddress == "" )
      gEvent.alarmEmailAddress = opener.getCharPref( opener.gCalendarWindow.calendarPreferences.calendarPref, "alarms.emailaddress", "" );

   if ( gEvent.alarmEmailAddress && gEvent.alarmEmailAddress != "" ) 
   {
      setFieldValue( "alarm-email-checkbox", true, "checked" );
      setFieldValue( "alarm-email-field", gEvent.alarmEmailAddress );
   }
   else
   {
      setFieldValue( "invite-checkbox", false, "checked" );
   }
   
   if ( gEvent.inviteEmailAddress != undefined && gEvent.inviteEmailAddress != "" ) 
   {
      setFieldValue( "invite-checkbox", true, "checked" );
      setFieldValue( "invite-email-field", gEvent.inviteEmailAddress );
   }
   else
   {
      setFieldValue( "invite-checkbox", false, "checked" );
   }
   
   setFieldValue( "repeat-checkbox", gEvent.recur, "checked");
   if( gEvent.recurInterval < 1 )
      gEvent.recurInterval = 1;

   setFieldValue( "repeat-length-field", gEvent.recurInterval );
   if( gEvent.recurUnits )
       setFieldValue( "repeat-length-units", gEvent.recurUnits );  //don't put the extra "value" element here, or it won't work.
   else
       setFieldValue( "repeat-length-units", "weeks" );

   setFieldValue( "repeat-forever-radio", (gEvent.recurForever != undefined && gEvent.recurForever != false), "selected" );
   
   setFieldValue( "repeat-until-radio", ( (gEvent.recurForever == undefined || gEvent.recurForever == false ) && gEvent.recurCount == 0), "selected" );

   setFieldValue( "repeat-numberoftimes-radio", (gEvent.recurCount != 0), "selected" );
   setFieldValue( "repeat-numberoftimes-textbox", gEvent.recurCount );
   
   
   /* Categories stuff */
   // Load categories
   var categoriesString = opener.GetUnicharPref(opener.gCalendarWindow.calendarPreferences.calendarPref, "categories.names", getDefaultCategories() );
   
   var categoriesList = categoriesString.split( "," );
   
   // insert the category already in the task so it doesn't get lost
   if( gEvent.categories )
   {
      if( categoriesString.indexOf( gEvent.categories ) == -1 )
         categoriesList[categoriesList.length] =  gEvent.categories;
   }

   // categoriesList.sort();

   var oldMenulist = document.getElementById( "categories-menulist-menupopup" );
   while( oldMenulist.hasChildNodes() )
      oldMenulist.removeChild( oldMenulist.lastChild );

   for (i = 0; i < categoriesList.length ; i++)
   {
      document.getElementById( "categories-field" ).appendItem(categoriesList[i], categoriesList[i]);
   }

   document.getElementById( "categories-field" ).selectedIndex = -1;
   setFieldValue( "categories-field", gEvent.categories );
   
   /* Server stuff */
   var serverList = opener.gCalendarWindow.calendarManager.calendars;
   document.getElementById( "server-menulist-menupopup" ).database.AddDataSource( opener.gCalendarWindow.calendarManager.rdf.getDatasource() );
   document.getElementById( "server-menulist-menupopup" ).builder.rebuild();
   
   if( args.mode == "new" )
   {
      if( args.server )
      {
         setFieldValue( "server-field", args.server );
      }
      else
      {
         document.getElementById( "server-field" ).selectedIndex = 1;
      }
   }
   else
   {
      if( gEvent.parent )
         setFieldValue( "server-field", gEvent.parent.server );
      else
      {
         document.getElementById( "server-field" ).selectedIndex = 1;
      }
         
      //for now you can't edit which file the event is in.
      setFieldValue( "server-field", "true", "disabled" );

      setFieldValue( "server-field-label", "true", "disabled" );
   }
   
   // update enabling and disabling
   updateRepeatItemEnabled();
   updateStartEndItemEnabled();
   updateAlarmItemEnabled();
   updateInviteItemEnabled();
      
   updateAddExceptionButton();

   //updateAlarmEmailItemEnabled();

   /*
   ** set the advanced weekly repeating stuff
   */
   setAdvancedWeekRepeat();
   
   setFieldValue( "advanced-repeat-dayofmonth", ( gEvent.recurWeekNumber == 0 || gEvent.recurWeekNumber == undefined ), "selected" );
   setFieldValue( "advanced-repeat-dayofweek", ( gEvent.recurWeekNumber > 0 && gEvent.recurWeekNumber != 5 ), "selected" );
   setFieldValue( "advanced-repeat-dayofweek-last", ( gEvent.recurWeekNumber == 5 ), "selected" );
   
   // start focus on title
   var firstFocus = document.getElementById( "title-field" );
   firstFocus.focus();

   opener.setCursor( "default" );
}



/**
*   Called when the OK button is clicked.
*/

function onOKCommand()
{
   // get values from the form and put them into the event
   
   gEvent.title       = getFieldValue( "title-field" );
   gEvent.description = getFieldValue( "description-field" );
   gEvent.location    = getFieldValue( "location-field" );
   if( getFieldValue( "status-field" ) != "" )
      gEvent.status      = eval( "gEvent."+getFieldValue( "status-field" ) );
   
   gEvent.allDay      = getFieldValue( "all-day-event-checkbox", "checked" );
   var startDate = document.getElementById( "start-date-picker" ).value;
   gEvent.start.year = startDate.getYear()+1900;
   gEvent.start.month = startDate.getMonth();
   gEvent.start.day = startDate.getDate();
   
   var startTime = getDateTimeFieldValue( "start-time-text" );
   gEvent.start.hour = startTime.getHours();
   gEvent.start.minute = startTime.getMinutes();
   
   var endDate = document.getElementById( "end-date-picker" ).value;
   //do this because the end date is always the same as the start date.
   gEvent.end.year = endDate.getYear()+1900;
   gEvent.end.month = endDate.getMonth();
   gEvent.end.day = endDate.getDate();

   var endTime = getDateTimeFieldValue( "end-time-text" );
   gEvent.end.hour = endTime.getHours();
   gEvent.end.minute = endTime.getMinutes();
   
   gEvent.url = getFieldValue( "uri-field" );

   gEvent.privateEvent = getFieldValue( "private-checkbox", "checked" );
   
   if( getFieldValue( "invite-checkbox", "checked" ) )
   {
      gEvent.inviteEmailAddress = getFieldValue( "invite-email-field", "value" );
   }
   else
   {
      gEvent.inviteEmailAddress = "";
   }
   gEvent.alarm       = getFieldValue( "alarm-checkbox", "checked" );
   gEvent.alarmLength = getFieldValue( "alarm-length-field" );
   gEvent.alarmUnits  = getFieldValue( "alarm-length-units", "value" );  

   dump( "!!!-->in calendarEventDialog.js, alarmUnits is "+gEvent.alarmUnits );
   if ( getFieldValue( "alarm-email-checkbox", "checked" ) ) 
   {
      gEvent.alarmEmailAddress = getFieldValue( "alarm-email-field", "value" );
      dump( "!!!-->in calendarEventDialog.js, alarmEmailAddress is "+gEvent.alarmEmailAddress );
   }
   else
   {
      gEvent.alarmEmailAddress = "";
   }

   gEvent.categories    = getFieldValue( "categories-field", "value" );

   gEvent.recur         = getFieldValue( "repeat-checkbox", "checked" );
   gEvent.recurUnits    = getFieldValue( "repeat-length-units", "value" );  
   gEvent.recurForever  = getFieldValue( "repeat-forever-radio", "selected" );
   gEvent.recurInterval = getFieldValue( "repeat-length-field" );
   gEvent.recurCount    = getFieldValue( "repeat-numberoftimes-textbox" );
   
   if( gEvent.recurInterval == 0 )
      gEvent.recur = false;

   var recurEndDate = document.getElementById( "repeat-end-date-picker" ).value;
   
   gEvent.recurEnd.setTime( recurEndDate );
   gEvent.recurEnd.hour = gEvent.start.hour;
   gEvent.recurEnd.minute = gEvent.start.minute;

   if( gEvent.recur == true )
   {
      //check that the repeat end time is later than the end time
      if( gEvent.recurUnits == "weeks" )
      {
         /*
         ** advanced weekly repeating, choosing the days to repeat
         */
         gEvent.recurWeekdays = getAdvancedWeekRepeat();
      }
      else if( gEvent.recurUnits == "months" )
      {
         /*
         ** advanced month repeating, either every day or every date
         */
         if( getFieldValue( "advanced-repeat-dayofweek", "selected" ) == true )
         {
            gEvent.recurWeekNumber = getWeekNumberOfMonth();
         } 
         else if( getFieldValue( "advanced-repeat-dayofweek-last", "selected" ) == true )
         {
            gEvent.recurWeekNumber = 5;
         }
         else
            gEvent.recurWeekNumber = 0;
      
      }
   }

   /* EXCEPTIONS */
   
   gEvent.removeAllExceptions();

   var listbox = document.getElementById( "exception-dates-listbox" );

   for( i = 0; i < listbox.childNodes.length; i++ )
   {
      dump( "\n exception added for "+listbox.childNodes[i].value );

      var dateObj = new Date( );

      dateObj.setTime( listbox.childNodes[i].value );

      gEvent.addException( dateObj );
   }

   /* File attachments */
   //loop over the items in the listbox
   gEvent.removeAttachments();

   var attachmentListbox = document.getElementById( "attachmentBucket" );

   for( i = 0; i < attachmentListbox.childNodes.length; i++ )
   {
      dump( "\n adding attachment to event added for "+attachmentListbox.childNodes[i].getAttribute( "label" ) );
      Attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance( Components.interfaces.nsIMsgAttachment );
	   
      Attachment.url = attachmentListbox.childNodes[i].getAttribute( "label" );
	
      gEvent.addAttachment( Attachment );
   }

    // Attach any specified contacts to the event
    if( gEventCardArray )
    {
        try{
            // Remove any existing contacts
            gEvent.removeContacts();
        
            // Add specified contacts
            for( var cardId in gEventCardArray )
            {
                if( gEventCardArray[ cardId ] )
                {
                    gEvent.addContact( gEventCardArray[ cardId ] );
                }
            }
        }
        catch( e )
        {

        }
    }

   var Server = getFieldValue( "server-field" );
   
   //if the end time is later than the start time... alert the user using text from the dtd.
   // call caller's on OK function
   gOnOkFunction( gEvent, Server );
      
   // tell standard dialog stuff to close the dialog
   return true;
}

function checkEndTime()
{
   var endDate = getDateTimeFieldValue( "end-time-text" );

   var startDate = getDateTimeFieldValue( "start-time-text" );

   var AllDayEvent = getFieldValue( "all-day-event-checkbox", "checked" );
   
   if( endDate.getTime() < startDate.getTime() && !AllDayEvent )
   {
      return( true );
   }
   else
   {
      return( false );
   }
}

/*
 * Check that the end date is after the start date, if they are the same day
 * then the checkEndTime function should catch the problem (if there is one).
 */

function checkEndDate()
{
   // Bad to get into floats.
   var startDate = Math.floor(document.getElementById( "start-date-picker" ).value.getTime()/86400000); 
   var endDate = Math.floor(document.getElementById( "end-date-picker" ).value.getTime()/86400000); 
   if ( endDate < startDate)
      return -1;
   else if (endDate > startDate)
      return 1;
   else 
      return 0;
}

function checkSetTimeDate()
{
   var CheckEndDate = checkEndDate();
   var CheckEndTime = checkEndTime();

   if ( CheckEndDate < 0 )
   {
      // end before start
      setDateError(true);
      setTimeError(false);
      return false;
   }
   else if ( CheckEndDate == 0 )
   {
      setDateError(false);
      // start & end same
      setTimeError(CheckEndTime);
      return !CheckEndTime;
   }
   else
   {
      setDateError(false);
      setTimeError(false);
      return true;
   }

}

/*
 * Check that the recurrence end date is after the end date of the event.
 * Unlike the time/date versions this one sets the error message too as is
 * doesn't depend on the outcome of any of the other tests
 */

function checkSetRecurTime()
{
   var recurEndDate = document.getElementById( "repeat-end-date-picker" ).value;

   var endDate = document.getElementById( "end-date-picker" ).value;

   var recurForever = getFieldValue( "repeat-forever-radio", "selected" );

   var recur = getFieldValue( "repeat-checkbox", "checked" );
   
   dump(recurForever+ " and "+ recur+ "\n"); 
   var state = (recurEndDate.getTime() < endDate.getTime() && !recurForever && recur) ;
   setRecurError(state);
   return(!state );
}

function setRecurError(state)
{
   document.getElementById("repeat-time-warning" ).setAttribute( "collapsed", !state);
}

function setDateError(state)
{ 
   document.getElementById( "end-date-warning" ).setAttribute( "collapsed", !state );
}

function setTimeError(state)
{ 
   document.getElementById( "end-time-warning" ).setAttribute( "collapsed", !state );
}

function setOkButton(state)
{
   if (state == false)
      document.getElementById( "calendar-new-eventwindow" ).getButton( "accept" ).setAttribute( "disabled", true );
   else
      document.getElementById( "calendar-new-eventwindow" ).getButton( "accept" ).removeAttribute( "disabled" );


}

function updateOKButton()
{
   var checkRecur = checkSetRecurTime();
   var checkTimeDate = checkSetTimeDate();
   setOkButton(checkRecur && checkTimeDate);
   this.sizeToContent();
}


/**
*   Called when a datepicker is finished, and a date was picked.
*/

function onDatePick( datepicker )
{
   var Now = new Date();

   //change the end date of recurring events to today, if the new date is after today and repeat is not checked.

   if ( datepicker.value > Now && !getFieldValue( "repeat-checkbox", "checked" ) ) 
   {
      document.getElementById( "repeat-end-date-picker" ).value = datepicker.value;
   }
   updateAdvancedWeekRepeat();
      
   updateAdvancedRepeatDayOfMonth();

   updateAddExceptionButton();

   updateOKButton();
}


/**
*   Called when an item with a time picker is clicked, BEFORE the picker is shown.
*/

function prepareTimePicker( timeFieldName )
{
   // get the popup and the field we are editing
   
   var timePickerPopup = document.getElementById( "oe-time-picker-popup" );
   var timeField = document.getElementById( timeFieldName );
   
   // tell the time picker the time to edit.
   setFieldValue( "oe-time-picker-popup", timeField.editDate, "value" );
   
   // remember the time field that is to be updated by adding a 
   // property "timeField" to the popup.
   
   timePickerPopup.timeField = timeField;
}


/**
*   Called when a timepicker is finished, and a time was picked.
*/

function onTimePick( timepopup )
{
   
   //get the new end time by adding on the time difference to the start time.
   newEndDate = timepopup.value.getTime() + gTimeDifference;
      
   newEndDate = new Date( newEndDate );

   if ( newEndDate.getDate() != timepopup.value.getDate() ) //if this moves us to tomorrow...
   {
      //put us back at today, with a time of 11:55 PM.
      document.getElementById( "end-date-picker" ).value = newEndDate;
   }
   
   if( timepopup.timeField.id != "end-time-text" )
   {
      setTimeFieldValue( "end-time-text", newEndDate );
   }
   
   
   if ( ( timepopup.timeField.id == "end-time-text" ) ) 
   {
      //if we are changing the end time, change the global duration.
      var EndTime = new Date( timepopup.value.getTime() );
      var StartTimeElement = document.getElementById( "start-time-text" );
      var StartTime = StartTimeElement.editDate;
      
      gTimeDifference = EndTime - StartTime; //the time difference in ms
      
      if ( gTimeDifference < 0 ) 
      {
         gTimeDifference = 0;
      }

   }
   
   // display the new time in the textbox

   timepopup.timeField.value = formatTime( timepopup.value );
   
   // remember the new date in a property, "editDate".  we created on the time textbox
   
   timepopup.timeField.editDate = timepopup.value;

   updateOKButton();
}


function updateEndDate( datepicker )
{
   //get the new end time by adding on the time difference to the start time.
   newEndDate = datepicker.value.getTime() + gDateDifference;
      
   newEndDate = new Date( newEndDate );

   document.getElementById( "end-date-picker" ).value = newEndDate;
}

/*
 * Called when the end date box has finished being editied
 */
 
function updateDateDifference( datepicker )
{
   gDateDifference = ( datepicker.value.getTime() - document.getElementById( "start-date-picker" ).value.getTime() );
   updateOKButton();
}

/**
*   Called when the repeat checkbox is clicked.
*/

function commandRepeat()
{
   updateRepeatItemEnabled();
}


/**
*   Called when the until radio is clicked.
*/

function commandUntil()
{
   updateUntilItemEnabled();

   updateOKButton();
}

/**
*   Called when the all day checkbox is clicked.
*/

function commandAllDay()
{
   updateStartEndItemEnabled();

   updateOKButton();
}

/**
*   Called when the alarm checkbox is clicked.
*/

function commandAlarm()
{
   document.getElementById( "alarm-email-checkbox" ).removeAttribute( "checked" );
   updateAlarmItemEnabled();
}


/**
*   Enable/Disable Alarm items
*/

function updateAlarmItemEnabled()
{
   var alarmCheckBox = "alarm-checkbox";
   
   var alarmField = "alarm-length-field";
   var alarmMenu = "alarm-length-units";
   var alarmLabel = "alarm-length-text";
      
   var alarmEmailCheckbox = "alarm-email-checkbox";
   var alarmEmailField = "alarm-email-field";

   if( getFieldValue(alarmCheckBox, "checked" ) || getFieldValue( alarmEmailCheckbox, "checked" ) )
   {
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
      setFieldValue( alarmCheckBox, true, "checked" );
      setFieldValue( alarmField, false, "disabled" );
      setFieldValue( alarmMenu, false, "disabled" );
      setFieldValue( alarmLabel, false, "disabled" );
      setFieldValue( alarmEmailCheckbox, false, "disabled" );
   }
   else
   {
      setFieldValue( alarmField, true, "disabled" );
      setFieldValue( alarmMenu, true, "disabled" );
      setFieldValue( alarmLabel, true, "disabled" );
      setFieldValue( alarmEmailField, true, "disabled" );
      setFieldValue( alarmEmailCheckbox, true, "disabled" );
      setFieldValue( alarmEmailCheckbox, false, "checked" );
   }
}


/**
*   Called when the alarm checkbox is clicked.
*/

function commandAlarmEmail()
{
   updateAlarmEmailItemEnabled();
}


/**
*   Enable/Disable Alarm items
*/

function updateAlarmEmailItemEnabled()
{
   var alarmCheckBox = "alarm-email-checkbox";
   
   var alarmEmailField = "alarm-email-field";
      
   if( getFieldValue( alarmCheckBox, "checked" ) )
   {
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
      
      setFieldValue( alarmEmailField, false, "disabled" );
   }
   else
   {
      setFieldValue( alarmEmailField, true, "disabled" );
   }
}


/**
*   Called when the alarm checkbox is clicked.
*/

function commandInvite()
{
   updateInviteItemEnabled();
}


/**
*   Enable/Disable Alarm items
*/

function updateInviteItemEnabled()
{
   var inviteCheckBox = document.getElementById( "invite-checkbox" );
   
   var inviteField = document.getElementById( "invite-email-field" );
   
   if( inviteCheckBox.checked )
   {
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
      
      inviteField.removeAttribute( "disabled" );
   }
   else
   {
      inviteField.setAttribute( "disabled", "true" );
   }
}


/**
*   Enable/Disable Repeat items
*/

function updateRepeatItemEnabled()
{
   var repeatCheckBox = document.getElementById( "repeat-checkbox" );
   
   var repeatDisableList = document.getElementsByAttribute( "disable-controller", "repeat" );
   
   if( repeatCheckBox.checked )
   {
      for( var i = 0; i < repeatDisableList.length; ++i )
      {
         if( repeatDisableList[i].getAttribute( "today" ) != "true" )
            repeatDisableList[i].removeAttribute( "disabled" );
      }
   }
   else
   {
      for( var j = 0; j < repeatDisableList.length; ++j )
         repeatDisableList[j].setAttribute( "disabled", "true" );
   }
   
   // udpate plural/singular
   updateRepeatPlural();
   
   updateAlarmPlural();
   
   // update until items whenever repeat changes
   updateUntilItemEnabled();
   
   // extra interface depending on units
   updateRepeatUnitExtensions();
}

/**
*   Update plural singular menu items
*/

function updateRepeatPlural()
{
    updateMenuPlural( "repeat-length-field", "repeat-length-units" );
}

/**
*   Update plural singular menu items
*/

function updateAlarmPlural()
{
    updateMenuPlural( "alarm-length-field", "alarm-length-units" );
}


/**
*   Update plural singular menu items
*/

function updateMenuPlural( lengthFieldId, menuId )
{
    var field = document.getElementById( lengthFieldId );
    var menu = document.getElementById( menuId );
   
    // figure out whether we should use singular or plural 
    
    var length = field.value;
    
    var newLabelNumber; 
    
    if( Number( length ) > 1  )
    {
        newLabelNumber = "labelplural"
    }
    else
    {
        newLabelNumber = "labelsingular"
    }
    
    // see what we currently show and change it if required
    
    var oldLabelNumber = menu.getAttribute( "labelnumber" );
    
    if( newLabelNumber != oldLabelNumber )
    {
        // remember what we are showing now
        
        menu.setAttribute( "labelnumber", newLabelNumber );
        
        // update the menu items
        
        var items = menu.getElementsByTagName( "menuitem" );
        
        for( var i = 0; i < items.length; ++i )
        {
            var menuItem = items[i];
            var newLabel = menuItem.getAttribute( newLabelNumber );
            menuItem.label = newLabel;
            menuItem.setAttribute( "label", newLabel );
            
        }
        
        // force the menu selection to redraw
        
        var saveSelectedIndex = menu.selectedIndex;
        menu.selectedIndex = -1;
        menu.selectedIndex = saveSelectedIndex;
    }
}

/**
*   Enable/Disable Until items
*/

function updateUntilItemEnabled()
{
   var repeatUntilRadio = document.getElementById( "repeat-until-radio" );
   var repeatCheckBox = document.getElementById( "repeat-checkbox" );
   
   var repeatEndText = document.getElementById( "repeat-end-date-picker" );
   
   if( repeatCheckBox.checked && repeatUntilRadio.selected  )
   {
      repeatEndText.removeAttribute( "disabled"  );
   }
   else
   {
      repeatEndText.setAttribute( "disabled", "true" );
   }
}


function updateRepeatUnitExtensions( )
{
   var repeatMenu = document.getElementById( "repeat-length-units" );
   var weekExtensions = document.getElementById( "repeat-extenstions-week" );
   var monthExtensions = document.getElementById( "repeat-extenstions-month" );

   //FIX ME! WHEN THE WINDOW LOADS, THIS DOESN'T EXIST
   if( repeatMenu.selectedItem )
   {
      switch( repeatMenu.selectedItem.value )
      {
           case "days":
               weekExtensions.setAttribute( "collapsed", "true" );
               monthExtensions.setAttribute( "collapsed", "true" );
           break;
           
           case "weeks":
               weekExtensions.removeAttribute( "collapsed" );
               monthExtensions.setAttribute( "collapsed", "true" );
               updateAdvancedWeekRepeat();
           break;
           
           case "months":
               weekExtensions.setAttribute( "collapsed", "true" );
               monthExtensions.removeAttribute( "collapsed" );
               //the following line causes resize problems after turning on repeating events
               updateAdvancedRepeatDayOfMonth();
           break;
           
           case "years":
               weekExtensions.setAttribute( "collapsed", "true" );
               monthExtensions.setAttribute( "collapsed", "true" );
           break;
       
      }
      sizeToContent();  
   }
   
}



/**
*   Enable/Disable Start/End items
*/

function updateStartEndItemEnabled()
{
   var allDayCheckBox = document.getElementById( "all-day-event-checkbox" );
   
   var startTimePicker = document.getElementById( "start-time-button" );
   var startTimeText = document.getElementById( "start-time-text" );
   
   var endTimePicker = document.getElementById( "end-time-button" );
   var endTimeText = document.getElementById( "end-time-text" );
   
   if( allDayCheckBox.checked )
   {
      // disable popups by removing the popup attribute
      
      startTimeText.setAttribute( "disabled", "true" );
      startTimeText.removeAttribute( "popup" );
      startTimePicker.setAttribute( "disabled", "true" );
      startTimePicker.removeAttribute( "popup" );
      
      endTimeText.setAttribute( "disabled", "true" );
      endTimeText.removeAttribute( "popup" );
      endTimePicker.setAttribute( "disabled", "true" );
      endTimePicker.removeAttribute( "popup" );
   }
   else
   {
      // enable popups by setting the popup attribute
      
      startTimeText.removeAttribute( "disabled" );
      startTimeText.setAttribute( "popup", "oe-time-picker-popup" );
      startTimePicker.removeAttribute( "disabled" );
      startTimePicker.setAttribute( "popup", "oe-time-picker-popup" );
      
      endTimeText.removeAttribute( "disabled" );
      endTimeText.setAttribute( "popup", "oe-time-picker-popup" );
      endTimePicker.removeAttribute( "disabled" );
      endTimePicker.setAttribute( "popup", "oe-time-picker-popup" );
   }
}




/**
*   Handle key down in repeat field
*/

function repeatLengthKeyDown( repeatField )
{
    updateRepeatPlural();
}

/**
*   Handle key down in alarm field
*/

function alarmLengthKeyDown( repeatField )
{
    updateAlarmPlural();
}


function repeatUnitCommand( repeatMenu )
{
    updateRepeatUnitExtensions();
}


/*
** Functions for advanced repeating elements
*/

function setAdvancedWeekRepeat()
{
   var checked = false;

   if( gEvent.recurWeekdays > 0 )
   {
      for( var i = 0; i < 7; i++ )
      {
         checked = ( ( gEvent.recurWeekdays | eval( "kRepeatDay_"+i ) ) == eval( gEvent.recurWeekdays ) );
         
         setFieldValue( "advanced-repeat-week-"+i, checked, "checked" );

         setFieldValue( "advanced-repeat-week-"+i, false, "today" );
      }
   }
  
   //get the day number for today.
   var dayNumber = document.getElementById( "start-date-picker" ).value.getDay();
   
   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "checked" );

   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "disabled" );

   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "today" );
   
}

/*
** Functions for advanced repeating elements
*/

function getAdvancedWeekRepeat()
{
   var Total = 0;

   for( var i = 0; i < 7; i++ )
   {
      if( getFieldValue( "advanced-repeat-week-"+i, "checked" ) == true )
      {
         Total += eval( "kRepeatDay_"+i );
      }
   }
   return( Total );
}

/*
** function to set the menu items text
*/
function updateAdvancedWeekRepeat()
{
   //get the day number for today.
   var dayNumber = document.getElementById( "start-date-picker" ).value.getDay();
   
   //uncheck them all if the repeat checkbox is checked
   var repeatCheckBox = document.getElementById( "repeat-checkbox" );
   
   if( repeatCheckBox.checked )
   {
      //uncheck them all
      for( var i = 0; i < 7; i++ )
      {
         setFieldValue( "advanced-repeat-week-"+i, false, "disabled" );
      
         setFieldValue( "advanced-repeat-week-"+i, false, "today" );
      }
   }

   if( !repeatCheckBox.checked )
   {
      for( i = 0; i < 7; i++ )
      {
         setFieldValue( "advanced-repeat-week-"+i, false, "checked" );
      }
   }

   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "checked" );

   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "disabled" );

   setFieldValue( "advanced-repeat-week-"+dayNumber, "true", "today" );
}

/*
** function to set the menu items text
*/
function updateAdvancedRepeatDayOfMonth()
{
   //get the day number for today.
   var dayNumber = document.getElementById( "start-date-picker" ).value.getDate();
   
   var dayExtension = getDayExtension( dayNumber );

   var weekNumber = getWeekNumberOfMonth();
   
   document.getElementById( "advanced-repeat-dayofmonth" ).setAttribute( "label", document.getElementById( "onthe-text" ).getAttribute( "value" )+dayNumber+dayExtension+document.getElementById( "ofthemonth-text" ).getAttribute( "value" ) );
   
   if( weekNumber == 4 && isLastDayOfWeekOfMonth() )
   {
      document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", getWeekNumberText( weekNumber )+" "+getDayOfWeek( dayNumber )+document.getElementById( "ofthemonth-text" ).getAttribute( "value" ) );

      document.getElementById( "advanced-repeat-dayofweek-last" ).removeAttribute( "collapsed" );

      document.getElementById( "advanced-repeat-dayofweek-last" ).setAttribute( "label", document.getElementById( "last-text" ).getAttribute( "value" )+getDayOfWeek( dayNumber )+document.getElementById( "ofthemonth-text" ).getAttribute( "value" ) );
   }
   else if( weekNumber == 4 && !isLastDayOfWeekOfMonth() )
   {
      document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", getWeekNumberText( weekNumber )+" "+getDayOfWeek( dayNumber )+document.getElementById( "ofthemonth-text" ).getAttribute( "value" ) );

      document.getElementById( "advanced-repeat-dayofweek-last" ).setAttribute( "collapsed", "true" );
   }
   else
   {
      document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", getWeekNumberText( weekNumber )+" "+getDayOfWeek( dayNumber )+document.getElementById( "ofthemonth-text" ).getAttribute( "value" ) );

      document.getElementById( "advanced-repeat-dayofweek-last" ).setAttribute( "collapsed", "true" );
   }
}

/*
** function to enable or disable the add exception button
*/
function updateAddExceptionButton()
{
   //get the date from the picker
   var datePickerValue = document.getElementById( "exceptions-date-picker" ).value;
   
   if( isAlreadyException( datePickerValue ) || document.getElementById( "repeat-checkbox" ).getAttribute( "checked" ) != "true" )
   {
      document.getElementById( "exception-add-button" ).setAttribute( "disabled", "true" );
   }
   else
   {
      document.getElementById( "exception-add-button" ).removeAttribute( "disabled" );
   }
}

function removeSelectedExceptionDate()
{
   var Listbox = document.getElementById( "exception-dates-listbox" );

   var SelectedItem = Listbox.selectedItem;

   if( SelectedItem )
      Listbox.removeChild( SelectedItem );
}

function addException( dateToAdd )
{
   if( !dateToAdd )
   {
      //get the date from the date and time box.
      //returns a date object
      var dateToAdd = document.getElementById( "exceptions-date-picker" ).value;
   }
   
   if( isAlreadyException( dateToAdd ) )
      return;

   var DateLabel = formatDate( dateToAdd );

   //add a row to the listbox
   document.getElementById( "exception-dates-listbox" ).appendItem( DateLabel, dateToAdd.getTime() );

   sizeToContent();
}

function isAlreadyException( dateObj )
{
   //check to make sure that the date is not already added.
   var listbox = document.getElementById( "exception-dates-listbox" );

   for( var i = 0; i < listbox.childNodes.length; i++ )
   {
      var dateToMatch = new Date( );
      
      dateToMatch.setTime( listbox.childNodes[i].value );
      if( dateToMatch.getMonth() == dateObj.getMonth() && dateToMatch.getFullYear() == dateObj.getFullYear() && dateToMatch.getDate() == dateObj.getDate() )
         return true;
   }
   return false;
}

function getDayExtension( dayNumber )
{
   switch( dayNumber )
   {
      case 1:
      case 21:
      case 31:
         return( "st" );
      case 2:
      case 22:
         return( "nd" );
      case 3:
      case 23:
         return( "rd" );
      default:
         return( "th" );
   }
}

function getDayOfWeek( )
{
   //get the day number for today.
   var dayNumber = document.getElementById( "start-date-picker" ).value.getDay();
   
   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

   //add one to the dayNumber because in the above prop. file, it starts at day1, but JS starts at 0
   var oneBasedDayNumber = parseInt( dayNumber ) + 1;
   
   return( dateStringBundle.GetStringFromName( "day."+oneBasedDayNumber+".name" ) );
   
}

function getWeekNumberOfMonth()
{
   //get the day number for today.
   var startTime = document.getElementById( "start-date-picker" ).value;
   
   var oldStartTime = startTime;

   var thisMonth = startTime.getMonth();
   
   var monthToCompare = thisMonth;

   var weekNumber = 0;

   while( monthToCompare == thisMonth )
   {
      startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );

      monthToCompare = startTime.getMonth();
      
      weekNumber++;
   }
   
   return( weekNumber );
}

function isLastDayOfWeekOfMonth()
{
   //get the day number for today.
   var startTime = document.getElementById( "start-date-picker" ).value;
   
   var oldStartTime = startTime;

   var thisMonth = startTime.getMonth();
   
   var monthToCompare = thisMonth;

   var weekNumber = 0;

   while( monthToCompare == thisMonth )
   {
      startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );

      monthToCompare = startTime.getMonth();
      
      weekNumber++;
   }
   
   if( weekNumber > 3 )
   {
      var nextWeek = new Date( oldStartTime.getTime() + ( 1000 * 60 * 60 * 24 * 7 ) );

      if( nextWeek.getMonth() != thisMonth )
      {
         //its the last week of the month
         return( true );
      }
   }

   return( false );
}

/* FILE ATTACHMENTS */

function removeSelectedAttachment()
{
   var Listbox = document.getElementById( "attachmentBucket" );

   var SelectedItem = Listbox.selectedItem;

   if( SelectedItem )
      Listbox.removeChild( SelectedItem );
}

function addAttachment( attachmentToAdd )
{
   if( !attachmentToAdd )
   {
      return;
   }
   
   //add a row to the listbox
   document.getElementById( "attachmentBucket" ).appendItem( attachmentToAdd.url, attachmentToAdd.url );

   sizeToContent();
}


function getWeekNumberText( weekNumber )
{
   switch( weekNumber )
   {
   case 1:
      return( "First" );
   case 2:
      return( "Second" );
   case 3:
      return( "Third" );
   case 4:
      return( "Fourth" );
   case 5:
      return( "Last" );
   default:
      return( false );
   }

}


var launch = true;

/* URL */
function launchBrowser()
{
   if( launch == false ) //stops them from clicking on it twice
      return;

   launch = false;
   //get the URL from the text box
   var UrlToGoTo = document.getElementById( "uri-field" ).value;
   
   if( UrlToGoTo.length < 4 ) //it has to be > 4, since it needs at least 1 letter, a . and a two letter domain name.
      return;

   //check if it has a : in it
   if( UrlToGoTo.indexOf( ":" ) == -1 )
      UrlToGoTo = "http://"+UrlToGoTo;

   //launch the browser to that URL
   window.open( UrlToGoTo, "calendar-opened-window" );
   launch = true;
}



/**
*   Helper function for filling the form, set the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to set 
*      newValue      - value to set property to ( if undefined no change is made )
*      propertyName  - OPTIONAL name of property to set, default is "value", use "checked" for 
*                               radios & checkboxes, "data" for drop-downs
*/

function setFieldValue( elementId, newValue, propertyName  )
{
   var undefined;
   
   if( newValue !== undefined )
   {
      var field = document.getElementById( elementId );
      
      if( newValue === false )
      {
         field.removeAttribute( propertyName );
      }
      else
      {
         if( propertyName )
         {
            field.setAttribute( propertyName, newValue );
         }
         else
         {
            field.value = newValue;
         }
      }
   }
}


/**
*   Helper function for getting data from the form, 
*   Get the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to get from 
*      propertyName  - OPTIONAL name of property to set, default is "value", use "checked" for 
*                               radios & checkboxes, "data" for drop-downs
*   RETURN
*      newValue      - value of property
*/

function getFieldValue( elementId, propertyName )
{
   var field = document.getElementById( elementId );
   
   if( propertyName )
   {
      return field[ propertyName ];
   }
   else
   {
      return field.value;
   }
}

/**
*   Helper function for getting a date/time from the form.
*   The element must have been set up with  setDateFieldValue or setTimeFieldValue.
*
* PARAMETERS
*      elementId     - ID of XUL element to get from 
* RETURN
*      newValue      - Date value of element
*/


function getDateTimeFieldValue( elementId )
{
   var field = document.getElementById( elementId );
   return field.editDate;
}



/**
*   Helper function for filling the form, set the value of a date field
*
* PARAMETERS
*      elementId     - ID of time textbox to set 
*      newDate       - Date Object to use
*/

function setDateFieldValue( elementId, newDate  )
{
   // set the value to a formatted date string 
   
   var field = document.getElementById( elementId );
   field.value = formatDate( newDate );
   
   // add an editDate property to the item to hold the Date object 
   // used in onDatePick to update the date from the date picker.
   // used in getDateTimeFieldValue to get the Date back out.
   
   // we clone the date object so changes made in place do not propagte 
   
   field.editDate = new Date( newDate );
}


/**
*   Helper function for filling the form, set the value of a time field
*
* PARAMETERS
*      elementId     - ID of time textbox to set 
*      newDate       - Date Object to use
*/

function setTimeFieldValue( elementId, newDate  )
{
   // set the value to a formatted time string 

   var field = document.getElementById( elementId );
   field.value = formatTime( newDate );
   
   // add an editDate property to the item to hold the Date object 
   // used in onTimePick to update the date from the time picker.
   // used in getDateTimeFieldValue to get the Date back out.
   
   // we clone the date object so changes made in place do not propagte 
   
   field.editDate = new Date( newDate );
}


/**
*   Take a Date object and return a displayable date string i.e.: May 5, 1959
*  :TODO: This should be moved into DateFormater and made to use some kind of
*         locale or user date format preference.
*/

function formatDate( date )
{
   return( opener.gCalendarWindow.dateFormater.getFormatedDate( date ) );
}


/**
*   Take a Date object and return a displayable time string i.e.: 12:30 PM
*/

function formatTime( time )
{
   var timeString = opener.gCalendarWindow.dateFormater.getFormatedTime( time );
   return timeString;
}


function debug( Text )
{
   dump( "\n"+ Text + "\n");

}
