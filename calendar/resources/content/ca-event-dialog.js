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
 * Contributor(s): Garth Smedley (garths@oeone.com) , Mike Potter (mikep@oeone.com)
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



/***** calendar/ca-event-dialog
* AUTHOR
*   Garth Smedley
* REQUIRED INCLUDES 
*   <script type="application/x-javascript" src="chrome://penglobal/content/dateUtils.js"/>
*   <script type="application/x-javascript" src="chrome://penglobal/content/calendarEvent.js"/>
*
* NOTES
*   Code for the calendar's new/edit event dialog.
*
*  Invoke this dialog to create a new event as follows:

   var args = new Object();
   args.mode = "new";               // "new" or "edit"
   args.onOk = <function>;          // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // newly creatd calendar event to be editted
    
   calendar.openDialog("caNewEvent", "chrome://calendar/content/ca-event-dialog.xul", true, args );
*
*  Invoke this dialog to edit an existing event as follows:
*
   var args = new Object();
   args.mode = "edit";                    // "new" or "edit"
   args.onOk = <function>;                // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // javascript object containin the event to be editted

   calendar.openDialog("caEditEvent", "chrome://calendar/content/ca-event-dialog.xul", true, args );

* When the user clicks OK the onOk function will be called with a calendar event object.
*
*  
* IMPLEMENTATION NOTES 
**********
*/


/*-----------------------------------------------------------------
*   W I N D O W      V A R I A B L E S
*/


var gDateFormatter = new DateFormater();  // used to format dates and times

var gEvent;          // event being edited
var gOnOkFunction;   // function to be called when user clicks OK

var gChangeEndTime = true; //
var gTimeDifference = 3600000;  //when editing an event, we change the end time if the start time is changing. This is the difference for the event time.

var gMode = ''; //what mode are we in? new or edit...

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
   
   gCategoryManager = args.CategoryManager;

   // remember function to call when OK is clicked
   
   gOnOkFunction = args.onOk;
   gEvent = args.calendarEvent;
   
   // mode is "new or "edit"
   // show proper header
   
   if( "new" == args.mode )
   {
      var titleDataItem = document.getElementById( "data-event-title-new" );
   }
   else
   {
      var titleDataItem = document.getElementById( "data-event-title-edit" );
   }
   
   var titleString = titleDataItem.getAttribute( "value" );
   var headerTitleItem = document.getElementById( "standard-dialog-title-text" );
   headerTitleItem.setAttribute( "value" , titleString );
   
   // show proper tip
   
   var tipNewItem = document.getElementById( "tip-new" );
   var tipEditItem = document.getElementById( "tip-edit" );
  
   if( "new" == args.mode )
   {
      tipNewItem.setAttribute( "collapsed", "false" );
      tipEditItem.setAttribute( "collapsed", "true" );
   }
   else
   {
      tipNewItem.setAttribute( "collapsed", "true" );
      tipEditItem.setAttribute( "collapsed", "false" );
   }
   
  
   //create the category drop down
   //if( !gCategoryManager )
   //   Categories = opener.gCategoryManager.getAllCategories();
   //else
   //   Categories = gCategoryManager.getAllCategories();
   Categories = new Array();

   var MenuPopup = document.getElementById( "category-field-menupopup" );
   for ( i in Categories ) 
   {
       var MenuItem = document.createElement( "menuitem" );
       MenuItem.setAttribute( "value", Categories[i].id );
       MenuItem.setAttribute( "label", Categories[i].name );
       MenuPopup.appendChild( MenuItem );
   }

   // fill in fields from the event
   
   setDateFieldValue( "start-date-text", gEvent.start );
   setTimeFieldValue( "start-time-text", gEvent.start );
   setTimeFieldValue( "end-time-text", gEvent.end );
   gTimeDifference = gEvent.end - gEvent.start; //the time difference in ms
   
   if ( gEvent.repeatForever ) 
   {
      gEvent.recurEnd = new Date();
   }
   setDateFieldValue( "repeat-end-date-text", gEvent.recurEnd );

   setFieldValue( "title-field", gEvent.title  );
   setFieldValue( "description-field", gEvent.description );
   setFieldValue( "location-field", gEvent.location );
   setFieldValue( "category-field", gEvent.category );
   
   setFieldValue( "all-day-event-checkbox", gEvent.allDay, "checked" );
   
   setFieldValue( "private-checkbox", gEvent.privateEvent, "checked" );
   
   setFieldValue( "alarm-checkbox", gEvent.alarm, "checked" );
   setFieldValue( "alarm-length-field", gEvent.alarmLength );
   setFieldValue( "alarm-length-units", gEvent.alarmUnits, "data" );  
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
   
   setFieldValue( "repeat-checkbox", gEvent.repeat, "checked");
   setFieldValue( "repeat-length-field", gEvent.recurInterval );
   setFieldValue( "repeat-length-units", gEvent.repeatUnits, "value" );  
   setFieldValue( "repeat-forever-radio", (gEvent.repeatForever != undefined && gEvent.repeatForever != false), "checked" );
   setFieldValue( "repeat-until-radio", (gEvent.repeatForever == undefined || gEvent.repeatForever == false), "checked" );
   
   // update enabling and disabling
   
   updateRepeatItemEnabled();
   updateStartEndItemEnabled();
   updateAlarmItemEnabled();
   updateInviteItemEnabled();
   
   // set up OK, Cancel
   
   doSetOKCancel( onOKCommand, 0 );

   // start focus on title
   
   var firstFocus = document.getElementById( "title-field" );
   firstFocus.focus();
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
   gEvent.category    = getFieldValue( "category-field" );
   
   gEvent.allDay      = getFieldValue( "all-day-event-checkbox", "checked" );
   gEvent.start       = getDateTimeFieldValue( "start-date-text" );
   
   var startTime      = getDateTimeFieldValue( "start-time-text" );
   gEvent.start.setHours( startTime.getHours() );
   gEvent.start.setMinutes( startTime.getMinutes() );
   gEvent.start.setSeconds( 0 );
   
   gEvent.end         = getDateTimeFieldValue( "end-time-text" );
   //do this because the end date is always the same as the start date.
   gEvent.end.setDate( gEvent.start.getDate() );
   gEvent.end.setMonth( gEvent.start.getMonth() );
   gEvent.end.setYear( gEvent.start.getFullYear() );
   
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
   debug( "!!!-->in ca-event-dialog.js, alarmUnits is "+gEvent.alarmUnits );
   if ( getFieldValue( "alarm-email-checkbox", "checked" ) ) 
   {
      gEvent.alarmEmailAddress = getFieldValue( "alarm-email-field", "value" );
      debug( "!!!-->in ca-event-dialog.js, alarmEmailAddress is "+gEvent.alarmEmailAddress );
   }
   else
   {
      gEvent.alarmEmailAddress = "";
   }
   gEvent.repeat         = getFieldValue( "repeat-checkbox", "checked" );
   gEvent.repeatUnits    = getFieldValue( "repeat-length-units", "value"  );  
   gEvent.repeatForever  = getFieldValue( "repeat-forever-radio", "checked" );
   gEvent.recurInterval  = getFieldValue( "repeat-length-field" );
   
   if( gEvent.recurInterval == 0 )
      gEvent.repeat = false;

   gEvent.recurEnd       = getDateTimeFieldValue( "repeat-end-date-text" );
   
   gEvent.recurEnd = new Date( gEvent.recurEnd.getFullYear(), gEvent.recurEnd.getMonth(), gEvent.recurEnd.getDate(), gEvent.start.getHours(), gEvent.start.getMinutes() );

   // :TODO: REALLY only do this if the alarm or start settings change.?
   gEvent.alarmWentOff = false;
   
   //if the end time is later than the start time... alert the user using text from the dtd.
   if ( gEvent.end < gEvent.start && !gEvent.allDay ) 
   {
      alert( neStartTimeErrorAlertMessage );
      return( false );
   }
   else
   {
      // call caller's on OK function
      gOnOkFunction( gEvent );
      
      // tell standard dialog stuff to close the dialog
      return true;
   }
}


/**
*   Called when an item with a datepicker is clicked, BEFORE the picker is shown.
*/

function prepareDatePicker( dateFieldName )
{
   // get the popup and the field we are editing
   
   var datePickerPopup = document.getElementById( "oe-date-picker-popup" );
   var dateField = document.getElementById( dateFieldName );
   
   // tell the date picker the date to edit.
   
   datePickerPopup.setAttribute( "value", dateField.editDate );
   
   // remember the date field that is to be updated by adding a 
   // property "dateField" to the popup.
   
   datePickerPopup.dateField = dateField;
}
   

/**
*   Called when a datepicker is finished, and a date was picked.
*/

function onDatePick( datepopup )
{
   // display the new date in the textbox
   
   datepopup.dateField.value = formatDate( datepopup.value );
   
   // remember the new date in a property, "editDate".  we created on the date textbox
   
   datepopup.dateField.editDate = datepopup.value;

   //change the end date of repeating events to today, if the new date is after today.

   var Now = new Date();

   if ( datepopup.value > Now ) 
   {
      document.getElementById( "repeat-end-date-text" ).value = formatDate( datepopup.value );
   }

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
   
   timePickerPopup.setAttribute( "value", timeField.editDate );
   
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
      newEndDate = new Date( timepopup.value.getFullYear(),
                             timepopup.value.getMonth(),
                             timepopup.value.getDate(),
                             23,
                             55,
                             0);
   }
   
   formattedEndTime = formatTime( newEndDate );
   if( timepopup.timeField.id != "end-time-text" )
   {
      document.getElementById( "end-time-text" ).value = formattedEndTime;
   
      document.getElementById( "end-time-text" ).editDate = newEndDate;
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
}

/**
*   Called when the all day checkbox is clicked.
*/

function commandAllDay()
{
   updateStartEndItemEnabled();
}

/**
*   Called when the alarm checkbox is clicked.
*/

function commandAlarm()
{
   updateAlarmItemEnabled();
}


/**
*   Enable/Disable Alarm items
*/

function updateAlarmItemEnabled()
{
   var alarmCheckBox = document.getElementById( "alarm-checkbox" );
   
   var alarmField = document.getElementById( "alarm-length-field" );
   var alarmMenu = document.getElementById( "alarm-length-units" );
   var alarmLabel = document.getElementById( "alarm-length-text" );
      
   var alarmEmailCheckbox = document.getElementById( "alarm-email-checkbox" );
   var alarmEmailField = document.getElementById( "alarm-email-field" );

   if( alarmCheckBox.checked )
   {
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
      
      alarmField.removeAttribute( "disabled" );
      alarmMenu.removeAttribute( "disabled"  );
      alarmLabel.removeAttribute( "disabled" );
      alarmEmailCheckbox.removeAttribute( "disabled" );
   }
   else
   {
      alarmField.setAttribute( "disabled", "true" );
      alarmMenu.setAttribute( "disabled", "true"  );
      alarmLabel.setAttribute( "disabled", "true" );
      alarmEmailField.setAttribute( "disabled", "true" );

      alarmEmailCheckbox.setAttribute( "disabled", "true" );
      alarmEmailCheckbox.setAttribute( "checked", false );
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
   var alarmCheckBox = document.getElementById( "alarm-email-checkbox" );
   
   var alarmEmailField = document.getElementById( "alarm-email-field" );
      
   if( alarmCheckBox.checked )
   {
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
      
      alarmEmailField.removeAttribute( "disabled" );
   }
   else
   {
      alarmEmailField.setAttribute( "disabled", "true" );
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
   
   var repeatField = document.getElementById( "repeat-length-field" );
   var repeatMenu = document.getElementById( "repeat-length-units" );
   var repeatGroup = document.getElementById( "repeat-until-group" );
   var repeatForever = document.getElementById( "repeat-forever-radio" );
   var repeatUntil = document.getElementById( "repeat-until-radio" );
   
   if( repeatCheckBox.checked )
   {
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
      
      repeatField.removeAttribute( "disabled" );
      repeatMenu.removeAttribute( "disabled" );
      repeatGroup.removeAttribute( "disabled" );
      repeatForever.removeAttribute( "disabled" );
      repeatUntil.removeAttribute( "disabled" );
   }
   else
   {
      repeatField.setAttribute( "disabled", "true" );
      repeatMenu.setAttribute( "disabled", "true" );
      repeatGroup.setAttribute( "disabled", "true" );
      repeatForever.setAttribute( "disabled", "true" );
      repeatUntil.setAttribute( "disabled", "true" );
   }
   
   // update until items whenever repeat changes
   
   updateUntilItemEnabled();
}


/**
*   Enable/Disable Until items
*/

function updateUntilItemEnabled()
{
   var repeatUntilRadio = document.getElementById( "repeat-until-radio" );
   var repeatCheckBox = document.getElementById( "repeat-checkbox" );
   
   var repeatEndText = document.getElementById( "repeat-end-date-text" );
   var repeatEndPicker = document.getElementById( "repeat-end-date-button" );
  
   if( repeatCheckBox.checked && repeatUntilRadio.checked  )
   {
      repeatEndText.removeAttribute( "disabled"  );
      repeatEndText.setAttribute( "popup", "oe-date-picker-popup" );
      repeatEndPicker.removeAttribute( "disabled" );
      repeatEndPicker.setAttribute( "popup", "oe-date-picker-popup" );
   }
   else
   {
      repeatEndText.setAttribute( "disabled", "true" );
      repeatEndText.removeAttribute( "popup" );
      repeatEndPicker.setAttribute( "disabled", "true" );
      repeatEndPicker.removeAttribute( "popup" );
   }
}


/**
*   Enable/Disable Start/End items
*/

function updateStartEndItemEnabled()
{
   var allDayCheckBox = document.getElementById( "all-day-event-checkbox" );
   
   var startTimeLabel = document.getElementById( "start-time-label" );
   var startTimePicker = document.getElementById( "start-time-button" );
   var startTimeText = document.getElementById( "start-time-text" );
   
   var endTimeLabel = document.getElementById( "end-time-label" );
   var endTimePicker = document.getElementById( "end-time-button" );
   var endTimeText = document.getElementById( "end-time-text" );
   
   if( allDayCheckBox.checked )
   {
      // disable popups by removing the popup attribute
      
      startTimeLabel.setAttribute( "disabled", "true" );
      startTimeText.setAttribute( "disabled", "true" );
      startTimeText.removeAttribute( "popup" );
      startTimePicker.setAttribute( "disabled", "true" );
      startTimePicker.removeAttribute( "popup" );
      
      endTimeLabel.setAttribute( "disabled", "true" );
      endTimeText.setAttribute( "disabled", "true" );
      endTimeText.removeAttribute( "popup" );
      endTimePicker.setAttribute( "disabled", "true" );
      endTimePicker.removeAttribute( "popup" );
   }
   else
   {
      // enable popups by setting the popup attribute
      
      startTimeLabel.removeAttribute( "disabled" );
      startTimeText.removeAttribute( "disabled" );
      startTimeText.setAttribute( "popup", "oe-time-picker-popup" );
      startTimePicker.removeAttribute( "disabled" );
      startTimePicker.setAttribute( "popup", "oe-time-picker-popup" );
      
      endTimeLabel.removeAttribute( "disabled" );
      endTimeText.removeAttribute( "disabled" );
      endTimeText.setAttribute( "popup", "oe-time-picker-popup" );
      endTimePicker.removeAttribute( "disabled" );
      endTimePicker.setAttribute( "popup", "oe-time-picker-popup" );
   }
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
      
      if( propertyName )
      {
         field[ propertyName ] = newValue;
      }
      else
      {
         field.value = newValue;
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
   var monthDayString = gDateFormatter.getFormatedDate( date );
   
   return  monthDayString + ", " + date.getFullYear();
}


/**
*   Take a Date object and return a displayable time string i.e.: 12:30 PM
*/

function formatTime( time )
{
   var timeString = gDateFormatter.getFormatedTime( time );
   return timeString;
}


function debug( Text )
{
   dump( "\n"+ Text + "\n");

}

