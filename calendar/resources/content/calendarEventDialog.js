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
 *                 Chris Charabaruk <coldacid@meldstar.com>
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
*   <script type="application/x-javascript" src="chrome://calendar/content/calendarEvent.js"/>
*
* NOTES
*   Code for the calendar's new/edit event dialog.
*
*  Invoke this dialog to create a new event as follows:

   var args = new Object();
   args.mode = "new";               // "new" or "edit"
   args.onOk = <function>;          // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // newly creatd calendar event to be editted
    
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


var gDateFormatter = new DateFormater();  // used to format dates and times

var gEvent;          // event being edited
var gOnOkFunction;   // function to be called when user clicks OK

var gTimeDifference = 3600000;  //when editing an event, we change the end time if the start time is changing. This is the difference for the event time.
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
   var endDate = new Date( gEvent.end.getTime() );
   setDateFieldValue( "start-date-text", startDate );
   setTimeFieldValue( "start-time-text", startDate );
   setTimeFieldValue( "end-time-text", endDate );
   gTimeDifference = gEvent.end.getTime() - gEvent.start.getTime(); //the time difference in ms
   
   if ( gEvent.recurForever ) 
   {
      var today = new Date();
      gEvent.recurEnd.setTime( startDate );
   }

   var recurEndDate = new Date( gEvent.recurEnd.getTime() );
   
   setDateFieldValue( "repeat-end-date-text", recurEndDate );

   setFieldValue( "title-field", gEvent.title  );
   setFieldValue( "description-field", gEvent.description );
   setFieldValue( "location-field", gEvent.location );
   
   setFieldValue( "all-day-event-checkbox", gEvent.allDay, "checked" );
   setFieldValue( "private-checkbox", gEvent.privateEvent, "checked" );
   
   if( gEvent.alarm === false && gEvent.alarmLength == 0 )
   {
      gEvent.alarmLength = gDefaultAlarmLength;
   }
   
   setFieldValue( "alarm-checkbox", gEvent.alarm, "checked" );
   setFieldValue( "alarm-length-field", gEvent.alarmLength );
   setFieldValue( "alarm-length-units", gEvent.alarmUnits );

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
   setFieldValue( "repeat-length-field", gEvent.recurInterval );
   setFieldValue( "repeat-length-units", gEvent.recurUnits );  //don't put the extra "value" element here, or it won't work.
   setFieldValue( "repeat-forever-radio", (gEvent.recurForever != undefined && gEvent.recurForever != false), "selected" );
   setFieldValue( "repeat-until-radio", (gEvent.recurForever == undefined || gEvent.recurForever == false), "selected" );
   
   // update enabling and disabling
   
   updateRepeatItemEnabled();
   updateStartEndItemEnabled();
   updateAlarmItemEnabled();
   updateInviteItemEnabled();
      
   //updateAlarmEmailItemEnabled();

   /*
   ** set the advanced weekly repeating stuff
   */
   setAdvancedWeekRepeat();
   
   setFieldValue( "advanced-repeat-dayofmonth", ( gEvent.recurWeekNumber == 0 || gEvent.recurWeekNumber == undefined ), "checked" );
   setFieldValue( "advanced-repeat-dayofweek", ( gEvent.recurWeekNumber > 0 ), "checked" );
   
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
   
   gEvent.allDay      = getFieldValue( "all-day-event-checkbox", "checked" );
   var startDate = getDateTimeFieldValue( "start-date-text" );
   gEvent.start.year = startDate.getYear()+1900;
   gEvent.start.month = startDate.getMonth();
   gEvent.start.day = startDate.getDate();
   
   var startTime = getDateTimeFieldValue( "start-time-text" );
   gEvent.start.hour = startTime.getHours();
   gEvent.start.minute = startTime.getMinutes();
   
   //do this because the end date is always the same as the start date.
   gEvent.end.year = gEvent.start.year;
   gEvent.end.month = gEvent.start.month;
   gEvent.end.day = gEvent.start.day;

   var endTime = getDateTimeFieldValue( "end-time-text" );
   gEvent.end.hour = endTime.getHours();
   gEvent.end.minute = endTime.getMinutes();
   
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
   gEvent.recur         = getFieldValue( "repeat-checkbox", "checked" );
   gEvent.recurUnits    = getFieldValue( "repeat-length-units", "value" );  
   gEvent.recurForever  = getFieldValue( "repeat-forever-radio", "selected" );
   gEvent.recurInterval  = getFieldValue( "repeat-length-field" );
   
   if( gEvent.recurInterval == 0 )
      gEvent.recur = false;

   var recurEndDate = getDateTimeFieldValue( "repeat-end-date-text" );
   
   gEvent.recurEnd.setTime( recurEndDate );
   gEvent.recurEnd.hour = gEvent.start.hour;
   gEvent.recurEnd.minute = gEvent.start.minute;

   if( gEvent.recur == true )
   {
      //check that the repeat end time is later than the end time
      if( recurEndDate.getTime() < gEvent.end.getTime() && gEvent.recurForever == false )
      {
         alert( neRecurErrorAlertMessage );
         return( false );
      }

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
         if( getFieldValue( "advanced-repeat-dayofweek", "checked" ) == true )
         {
            gEvent.recurWeekNumber = getWeekNumberOfMonth();
         }
         else
            gEvent.recurWeekNumber = 0;
      
      }
   }

   
   
   // :TODO: REALLY only do this if the alarm or start settings change.?
   //if the end time is later than the start time... alert the user using text from the dtd.

   if ( gEvent.end.getTime() < gEvent.start.getTime() && !gEvent.allDay ) 
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
   
   setFieldValue( "oe-date-picker-popup", dateField.editDate, "value" );
   
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

   var Now = new Date();

   //change the end date of recurring events to today, if the new date is after today and repeat is not checked.

   if ( datepopup.value > Now && !getFieldValue( "repeat-checkbox", "checked" ) ) 
   {
      document.getElementById( "repeat-end-date-text" ).value = formatDate( datepopup.value );

      document.getElementById( "repeat-end-date-text" ).editDate = datepopup.value;
   }

   updateAdvancedWeekRepeat();
      
   updateAdvancedRepeatDayOfMonth();
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
      setFieldValue( "end-time-text", formattedEndTime, "value" );

      setFieldValue( "end-time-text", newEndDate, "editDate" );
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
   var alarmCheckBox = "alarm-checkbox";
   
   var alarmField = "alarm-length-field";
   var alarmMenu = "alarm-length-units";
   var alarmLabel = "alarm-length-text";
      
   var alarmEmailCheckbox = "alarm-email-checkbox";
   var alarmEmailField = "alarm-email-field";

   if( getFieldValue(alarmCheckBox, "checked" ) )
   {
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
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
      // call remove attribute beacuse some widget code checks for the presense of a 
      // disabled attribute, not the value.
      for( var i = 0; i < repeatDisableList.length; ++i )
      {
         if( repeatDisableList[i].getAttribute( "today" ) != "true" )
            repeatDisableList[i].removeAttribute( "disabled" );
      }
   }
   else
   {
      for( var j = 0; j < repeatDisableList.length; ++j )
      {
         repeatDisableList[j].setAttribute( "disabled", "true" );
      }
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
   
   var repeatEndText = document.getElementById( "repeat-end-date-text" );
   var repeatEndPicker = document.getElementById( "repeat-end-date-button" );
  
   //RADIO REQUIRES SELECTED NOT CHECKED
   if( repeatCheckBox.checked && repeatUntilRadio.selected  )
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
               weekExtensions.setAttribute( "collapsed", "false" );
               monthExtensions.setAttribute( "collapsed", "true" );
               updateAdvancedWeekRepeat();
           break;
           
           case "months":
               weekExtensions.setAttribute( "collapsed", "true" );
               monthExtensions.setAttribute( "collapsed", "false" );
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
      for( var i = 0; i < 6; i++ )
      {
         dump( gEvent.recurWeekdays | eval( "kRepeatDay_"+i ) );
   
         checked = ( ( gEvent.recurWeekdays | eval( "kRepeatDay_"+i ) ) == eval( gEvent.recurWeekdays ) );
         
         dump( "checked is "+checked );
         
         setFieldValue( "advanced-repeat-week-"+i, checked, "checked" );

         setFieldValue( "advanced-repeat-week-"+i, false, "today" );
      }
   }
  
   //get the day number for today.
   var startTime = getDateTimeFieldValue( "start-date-text" );
   
   var dayNumber = startTime.getDay();
   
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
   var startTime = getDateTimeFieldValue( "start-date-text" );
   
   var dayNumber = startTime.getDay();

   //uncheck them all if the repeat checkbox is checked
   var repeatCheckBox = document.getElementById( "repeat-checkbox" );
   
   if( repeatCheckBox.checked )
   {
      //uncheck them all
      for( var i = 0; i < 7; i++ )
      {
         setFieldValue( "advanced-repeat-week-"+i, false, "checked" );

         setFieldValue( "advanced-repeat-week-"+i, false, "disabled" );
      
         setFieldValue( "advanced-repeat-week-"+i, false, "today" );
   
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
   var startTime = getDateTimeFieldValue( "start-date-text" );
   
   var dayNumber = startTime.getDate();

   var dayExtension = getDayExtension( dayNumber );

   document.getElementById( "advanced-repeat-dayofmonth" ).setAttribute( "label", "On the "+dayNumber+dayExtension+" of the month" );

   document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", getWeekNumberText( getWeekNumberOfMonth() )+" "+getDayOfWeek( dayNumber )+" of the month" );
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
   var startTime = getDateTimeFieldValue( "start-date-text" );
   
   var dayNumber = startTime.getDay();

   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

   //add one to the dayNumber because in the above prop. file, it starts at day1, but JS starts at 0
   var oneBasedDayNumber = parseInt( dayNumber ) + 1;
   
   return( dateStringBundle.GetStringFromName( "day."+oneBasedDayNumber+".name" ) );
   
}

function getWeekNumberOfMonth()
{
   //get the day number for today.
   var startTime = getDateTimeFieldValue( "start-date-text" );
   
   var dayNumber = startTime.getDay();
   
   var thisMonth = startTime.getMonth();
   
   var monthToCompare = startTime.getMonth();

   var weekNumber = 0;

   while( monthToCompare == thisMonth )
   {
      startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );

      monthToCompare = startTime.getMonth();
      
      weekNumber++;
   }
   
   if( weekNumber > 3 )
   {
      var nextWeek = new Date( getDateTimeFieldValue( "start-date-text" ).getTime() + ( 1000 * 60 * 60 * 24 * 7 ) );

      if( nextWeek.getMonth() != thisMonth )
      {
         //its the last week of the month
         weekNumber = 5;
      }
   }

   return( weekNumber );
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
