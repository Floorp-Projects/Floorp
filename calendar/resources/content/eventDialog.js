/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Mostafa Hosseini <mostafah@oeone.com>
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



/***** calendar/eventDialog.js
* AUTHOR
*   Garth Smedley
* REQUIRED INCLUDES 
*   <script type="application/x-javascript" src="chrome://calendar/content/dateUtils.js"/>
*   <script type="application/x-javascript" src="chrome://calendar/content/applicationUtil.js"/>
* NOTES
*   Code for the calendar's new/edit event dialog.
*
*  Invoke this dialog to create a new event as follows:

   var args = new Object();
   args.mode = "new";               // "new" or "edit"
   args.onOk = <function>;          // funtion to call when OK is clicked
   args.calendarEvent = calendarEvent;    // newly creatd calendar event to be editted
   
   openDialog("chrome://calendar/content/eventDialog.xul", "caEditEvent", "chrome,titlebar,modal", args );
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

var debugenabled=true;

var gEvent;          // event being edited
var gOnOkFunction;   // function to be called when user clicks OK

var gDuration = -1;   // used to preserve duration when changing event start.

const DEFAULT_ALARM_LENGTH = 15; //default number of time units, an alarm goes off before an event

const kRepeatDay_0 = 1<<0;//Sunday
const kRepeatDay_1 = 1<<1;//Monday
const kRepeatDay_2 = 1<<2;//Tuesday
const kRepeatDay_3 = 1<<3;//Wednesday
const kRepeatDay_4 = 1<<4;//Thursday
const kRepeatDay_5 = 1<<5;//Friday
const kRepeatDay_6 = 1<<6;//Saturday


var gStartDate = new Date( );
// Note: gEndDate is *exclusive* end date, so duration = end - start.
// For all-day(s) events (no times), displayed end date is one day earlier.
var gEndDate = new Date( );

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

    gOnOkFunction = args.onOk;
    // XXX I want to get rid of the use of gEvent
    gEvent = args.calendarEvent;


    event = args.calendarEvent;

    // mode is "new or "edit" - show proper header
    var titleDataItem = null;

    if ("new" == args.mode)
        title = document.getElementById("data-event-title-new").getAttribute("value");
    else {
        title = document.getElementById("data-event-title-edit").getAttribute("value");
    }

    document.getElementById("calendar-new-component-window").setAttribute("title", title);


    // fill in fields from the event
    gStartDate = event.startDate.jsDate;
    document.getElementById("start-datetime").value = gStartDate;

    gEndDate = event.endDate.jsDate;
    var displayEndDate = new Date(gEndDate);
    if (event.isAllDay) {
        //displayEndDate == icalEndDate - 1, in the case of allday events
        displayEndDate.setDate(displayEndDate.getDate() - 1);
    }
    document.getElementById("end-datetime").value = displayEndDate;

    gDuration = gEndDate.getTime() - gStartDate.getTime(); //in ms

    /*
    if (event.recurForever) {
        event.recurEnd.setTime(gEndDate);
    }
    
    //do the stuff for exceptions
    var ArrayOfExceptions = event.getExceptions();

    while( ArrayOfExceptions.hasMoreElements() ) {
        var ExceptionTime = ArrayOfExceptions.getNext().QueryInterface(Components.interfaces.nsISupportsPRTime).data;

        var ExceptionDate = new Date( ExceptionTime );

        addException( ExceptionDate );
    }

    //file attachments;
    for( var i = 0; i < event.attachmentsArray.Count(); i++ ) {
        var thisAttachment = event.attachmentsArray.QueryElementAt( i, Components.interfaces.nsIMsgAttachment );

        addAttachment( thisAttachment );
    }
    */

    document.getElementById("exceptions-date-picker").value = gStartDate;

    dump("event: "+event+"\n");
    setFieldValue("title-field",       event.title  );
    setFieldValue("description-field", event.getProperty("description"));
    setFieldValue("location-field",    event.getProperty("location"));
    setFieldValue("uri-field",         event.getProperty("url"));

    switch(event.status) {
    case event.CAL_ITEM_STATUS_TENTATIVE:
        setFieldValue("event-status-field", "ICAL_STATUS_TENTATIVE");
        break;
    case event.CAL_ITEM_STATUS_CONFIRMED:
        setFieldValue("event-status-field", "ICAL_STATUS_CONFIRMED");
        break;
    case event.CAL_ITEM_STATUS_CANCELLED:
        setFieldValue("event-status-field", "ICAL_STATUS_CANCELLED");
        break;
    }

    setFieldValue("all-day-event-checkbox", event.isAllDay,  "checked");
    setFieldValue("private-checkbox",       event.isPrivate, "checked");

    if (!event.hasAlarm) {
        // If the event has no alarm
        setFieldValue("alarm-type", "none");
    }    
    else {
        setFieldValue("alarm-length-field",     event.getProperty("alarmLength"));
        setFieldValue("alarm-length-units",     event.getProperty("alarmUnits"));
        setFieldValue("alarm-trigger-relation", event.getProperty("ICAL_RELATED_PARAMETER"));
        // If the event has an alarm email address, assume email alarm type
        var alarmEmailAddress = event.getProperty("alarmEmailAddress");
        if (alarmEmailAddress && alarmEmailAddress != "") {
            setFieldValue("alarm-type", "email");
            setFieldValue("alarm-email-field", alarmEmailAddress);
        }
        else {
            setFieldValue("alarm-type", "popup");
            /* XXX lilmatt: finish this by selection between popup and 
               popupAndSound by checking pref "calendar.alarms.playsound" */
        }
    }

    var inviteEmailAddress = event.getProperty("inviteEmailAddress");
    if (inviteEmailAddress != undefined && inviteEmailAddress != "") {
        setFieldValue("invite-checkbox", true, "checked");
        setFieldValue("invite-email-field", inviteEmailAddress);
    }
    else {
        setFieldValue("invite-checkbox", false, "checked");
    }

    /* XXX
    setFieldValue("repeat-checkbox", gEvent.recur, "checked");
    if( gEvent.recurInterval < 1 )
        gEvent.recurInterval = 1;

    setFieldValue( "repeat-length-field", gEvent.recurInterval );
    if( gEvent.recurUnits )
        setFieldValue( "repeat-length-units", gEvent.recurUnits );  //don't put the extra "value" element here, or it won't work.
    else
        setFieldValue( "repeat-length-units", "weeks" );

    if ( gEvent.recurEnd && gEvent.recurEnd.isSet )
        setFieldValue( "repeat-end-date-picker", new Date( gEvent.recurEnd.getTime() ) );
    else
        setFieldValue( "repeat-end-date-picker", new Date() ); // now
    
    setFieldValue( "repeat-forever-radio", (gEvent.recurForever != undefined && gEvent.recurForever != false), "selected" );
    
    setFieldValue( "repeat-until-radio", ( (gEvent.recurForever == undefined || gEvent.recurForever == false ) && gEvent.recurCount == 0), "selected" );
 
    setFieldValue( "repeat-numberoftimes-radio", (gEvent.recurCount != 0), "selected" );
    setFieldValue( "repeat-numberoftimes-textbox", Math.max(gEvent.recurCount, 1));
    */


    /* Categories stuff */

    // Load categories
    /*
    var categoriesString = opener.GetUnicharPref(opener.gCalendarWindow.calendarPreferences.calendarPref, "categories.names", getDefaultCategories());

    var categoriesList = categoriesString.split( "," );

    // insert the category already in the task so it doesn't get lost
    var categories = event.getProperty("categories");
    if (categories) {
        if (categoriesString.indexOf(categories) == -1)
            categoriesList[categoriesList.length] = categories;
    }

    categoriesList.sort();

    var oldMenulist = document.getElementById( "categories-menulist-menupopup" );
    while( oldMenulist.hasChildNodes() )
        oldMenulist.removeChild( oldMenulist.lastChild );

    for (i = 0; i < categoriesList.length ; i++)
    {
        document.getElementById( "categories-field" ).appendItem(categoriesList[i], categoriesList[i]);
    }

    document.getElementById( "categories-field" ).selectedIndex = -1;
    setFieldValue( "categories-field", gEvent.categories );
   

    // Server stuff
    document.getElementById( "server-menulist-menupopup" ).database.AddDataSource( opener.gCalendarWindow.calendarManager.rdf.getDatasource() );
    document.getElementById( "server-menulist-menupopup" ).builder.rebuild();
   
    if (args.mode == "new") {
        if ("server" in args) {
            setFieldValue( "server-field", args.server );
        }
        else {
            document.getElementById( "server-field" ).selectedIndex = 1;
        }
    } else {
        if (gEvent.parent)
            setFieldValue( "server-field", gEvent.parent.server );
        else
            document.getElementById( "server-field" ).selectedIndex = 1;
          
        //for now you can't edit which file the event is in.
        setFieldValue( "server-field", "true", "disabled" );
        setFieldValue( "server-field-label", "true", "disabled" );
    }
    */


    // update enabling and disabling
    updateRepeatItemEnabled();
    updateStartEndItemEnabled();
    updateInviteItemEnabled();

    updateAddExceptionButton();

    //set the advanced weekly repeating stuff
    setAdvancedWeekRepeat();

    /*
    setFieldValue("advanced-repeat-dayofmonth", (gEvent.recurWeekNumber == 0 || gEvent.recurWeekNumber == undefined), "selected");
    setFieldValue("advanced-repeat-dayofweek", (gEvent.recurWeekNumber > 0 && gEvent.recurWeekNumber != 5), "selected");
    setFieldValue("advanced-repeat-dayofweek-last", (gEvent.recurWeekNumber == 5), "selected");
    */

    // hide/show fields and widgets for item type
    processComponentType();

    // hide/show fields and widgets for alarm type
    processAlarmType();

    // start focus on title
    var firstFocus = document.getElementById("title-field");
    firstFocus.focus();

    // revert cursor from "wait" set in calendar.js editEvent, editNewEvent
    opener.setCursor( "auto" );

    self.focus();
}



/**
*   Called when the OK button is clicked.
*/

function onOKCommand()
{
    event = gEvent;

    // if this event isn't mutable, we need to clone it like a sheep
    var originalEvent = event;
    if (!event.isMutable)
        event = originalEvent.clone();

    // get values from the form and put them into the event
    // calIEvent properties
    //event.startDate.jsDate = gStartDate;
    //event.endDate.jsDate   = gEndDate;
    event.isAllDay = getFieldValue( "all-day-event-checkbox", "checked" );


    // calIItemBase properties
    event.title = getFieldValue( "title-field" );
    event.isPrivate = getFieldValue( "private-checkbox", "checked" );
    if (getFieldValue( "alarm-type" ) != "" && getFieldValue( "alarm-type" ) != "none")
        event.hasAlarm == 1
    if (event.hasAlarm) {
        alarmLength = getFieldValue( "alarm-length-field" );
        alarmUnits  = getFieldValue( "alarm-length-units", "value" );
    //event.alarmTime = ...
    }

    event.recurrenceInfo = null;
    
    if (getFieldValue("repeat-checkbox", "checked")) {
        recurrenceInfo = createRecurrenceInfo();
        recurUnits    = getFieldValue("repeat-length-units", "value");
    recurForever  = getFieldValue("repeat-forever-radio", "selected");
    recurInterval = getFieldValue("repeat-length-field");
    recurCount    = (getFieldValue("repeat-numberoftimes-radio", "selected")
        ? Math.max(1, getFieldValue("repeat-numberoftimes-textbox"))
        : 0); // 0 means not selected.

    //recurrenceInfo.recurType = ...
    //recurrenceInfo.recurEnd = ...
    }



    // other properties
    event.setProperty('categories',  getFieldValue("categories-field", "value"));
    event.setProperty('description', getFieldValue("description-field"));
    event.setProperty('location',    getFieldValue("location-field"));
    event.setProperty('url',         getFieldValue("uri-field"));

    event.setProperty("ICAL_RELATED_PARAMETER", getFieldValue("alarm-trigger-relation", "value"));

    if (getFieldValue("alarm-type") == "email" )
        event.setProperty('alarmEmailAddress', getFieldValue("alarm-email-field", "value"));
    else
        event.deleteProperty('alarmEmailAddress');


    /*
      if( getFieldValue( "status-field" ) != "" )
          gEvent.status      = eval( "gEvent."+getFieldValue( "status-field" ) );
    */

    if (getFieldValue("invite-checkbox", "checked"))
        event.setProperty('inviteEmailAddress', getFieldValue("invite-email-field", "value"));
    else
        event.deleteProperty('inviteEmailAddress');


   /* EXCEPTIONS */
   
/*
   gEvent.removeAllExceptions();

   var listbox = document.getElementById( "exception-dates-listbox" );

   var i;
   for( i = 0; i < listbox.childNodes.length; i++ )
   {
      var dateObj = new Date( );

      dateObj.setTime( listbox.childNodes[i].value );

      gEvent.addException( dateObj );
   }
*/

    /* File attachments */
    //loop over the items in the listbox
    //event.attachments.clear();

    var attachmentListbox = document.getElementById( "attachmentBucket" );

    for (i = 0; i < attachmentListbox.childNodes.length; i++) {
        attachment = Components.classes["@mozilla.org/messengercompose/attachment;1"].createInstance(Components.interfaces.nsIMsgAttachment);
    attachment.url = attachmentListbox.childNodes[i].getAttribute("label");
    // XXX
    //event.attachments.appendElement(attachment);
    }

    // Attach any specified contacts to the event
    if (gEventCardArray) {
        try {
            // Remove any existing contacts
            //event.contacts.clear();

            // Add specified contacts
            for (var cardId in gEventCardArray)
                if (gEventCardArray[cardId]) {
                    // XXX
                    //event.contacts.appendElement(gEventCardArray[cardId]);
                }
        }
        catch (e)
        {
        }
    }

   var Server = getFieldValue( "server-field" );

   // :TODO: REALLY only do this if the alarm or start settings change.?
   //if the end time is later than the start time... alert the user using text from the dtd.
   // call caller's on OK function

   gOnOkFunction(event, Server, originalEvent);

   // tell standard dialog stuff to close the dialog
   return true;
}

/*
 * Compare dateA with dateB ignoring time of day of each date object.
 * Comparison based on year, month, and day, ignoring rest.
 * Returns
 *   -1 if dateA <  dateB (ignoring time of day)
 *    0 if dateA == dateB (ignoring time of day)
 *   +1 if dateA >  dateB (ignoring time of day)
 */

function compareIgnoringTimeOfDay(dateA, dateB)
{
  if (dateA.getFullYear() == dateB.getFullYear() &&
      dateA.getMonth() == dateB.getMonth() &&
      dateA.getDate() == dateB.getDate() ) {
    return 0;
  } else if (dateA < dateB) {
      return -1;
  } else if (dateA > dateB) {
      return 1;
   }
}

/** Check that end date is not before start date, and update warning msg.
    Return true if problem found. **/
function checkSetTimeDate()
{
   var dateComparison = compareIgnoringTimeOfDay(gEndDate, gStartDate);

   if (dateComparison < 0 ||
       (dateComparison == 0 && getFieldValue( "all-day-event-checkbox",
                                              "checked" )))
   {
      // end before start, or all day event and end date is not exclusive.
      setDateError(true);
      setTimeError(false);
      return false;
   }
   else if (dateComparison == 0)
   {
      setDateError(false);
      // start & end date same, so compare entire time (ms since 1970)
      var isBadEndTime = gEndDate.getTime() < gStartDate.getTime();
      setTimeError(isBadEndTime);
      return !isBadEndTime;
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

function checkSetRecur()
{
   var recur = getFieldValue( "repeat-checkbox", "checked" );
   var recurUntil = getFieldValue( "repeat-until-radio", "selected" );
   var recurUntilDate = document.getElementById("repeat-end-date-picker").value;
   
   var untilDateIsBeforeEndDate =
     ( recur && recurUntil && 
       // until date is less than end date if total milliseconds time is less
       recurUntilDate.getTime() < gEndDate.getTime() && 
       // if on same day, ignore time of day for now
       // (may change in future for repeats within a day, such as hourly)
       !( recurUntilDate.getFullYear() == gEndDate.getFullYear() &&
          recurUntilDate.getMonth() == gEndDate.getMonth() &&
          recurUntilDate.getDate() == gEndDate.getDate() ));
   setRecurError(untilDateIsBeforeEndDate);
   return(!untilDateIsBeforeEndDate);
}

function setRecurError(state)
{
   document.getElementById("repeat-time-warning" ).setAttribute( "hidden", !state);
}

function setDateError(state)
{ 
   document.getElementById( "end-date-warning" ).setAttribute( "hidden", !state );
}

function setTimeError(state)
{ 
   document.getElementById( "end-time-warning" ).setAttribute( "hidden", !state );
}

function setOkButton(state)
{
   if (state == false)
      document.getElementById( "calendar-new-component-window" ).getButton( "accept" ).setAttribute( "disabled", true );
   else
      document.getElementById( "calendar-new-component-window" ).getButton( "accept" ).removeAttribute( "disabled" );
}

function updateOKButton()
{
   var checkRecur = checkSetRecur();
   var checkTimeDate = checkSetTimeDate();
   setOkButton(checkRecur && checkTimeDate);
   this.sizeToContent();
}


/**
*   Called when a datepicker is finished, and a date was picked.
*/

function onDateTimePick( dateTimePicker )
{
   var pickedDateTime = new Date( dateTimePicker.value );

   if( dateTimePicker.id == "end-datetime" )
   {
     if( getFieldValue( "all-day-event-checkbox", "checked" ) ) {
       //display enddate == ical enddate - 1 (for allday events)
       pickedDateTime.setDate( pickedDateTime.getDate() + 1 );
     }
     gEndDate = pickedDateTime;
     // save the duration
     gDuration = gEndDate.getTime() - gStartDate.getTime();

     updateOKButton();
     return;
   }

   if( dateTimePicker.id == "start-datetime" )
   {
     gStartDate = pickedDateTime;
     // preserve the previous duration by changing end
     gEndDate.setTime( gStartDate.getTime() + gDuration );
     
     var displayEndDate = new Date(gEndDate)
     if( getFieldValue( "all-day-event-checkbox", "checked" ) ) {
       //display enddate == ical enddate - 1 (for allday events)
       displayEndDate.setDate( displayEndDate.getDate() - 1 );
     }
     document.getElementById( "end-datetime" ).value = displayEndDate;
   }

   var now = new Date();

   // change the end date of recurring events to today, 
   // if the new date is after today and repeat is not checked.
   if ( pickedDateTime.getTime() > now.getTime() &&
        !getFieldValue( "repeat-checkbox", "checked" ) ) 
   {
     document.getElementById( "repeat-end-date-picker" ).value = pickedDateTime;
   }
   updateAdvancedWeekRepeat();
   updateAdvancedRepeatDayOfMonth();
   updateAddExceptionButton();
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
  //user enddate == ical enddate - 1 (for allday events)
  if( getFieldValue( "all-day-event-checkbox", "checked" ) ) {
    gEndDate.setDate( gEndDate.getDate() + 1 );
  } else { 
    gEndDate.setDate( gEndDate.getDate() - 1 );
  }
  // update the duration
  gDuration = gEndDate.getTime() - gStartDate.getTime();

   updateStartEndItemEnabled();
   updateOKButton();
}


/**
*   Called when the invite checkbox is clicked.
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
    
    // XXX This assumes that "0 days, minutes, etc." is plural in other languages.
    if( ( Number( length ) == 0 ) || ( Number( length ) > 1 ) )
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
   var repeatCheckBox = document.getElementById( "repeat-checkbox" );
   var numberRadio = document.getElementById("repeat-numberoftimes-radio");
   var numberTextBox = document.getElementById("repeat-numberoftimes-textbox");
   
   if( repeatCheckBox.checked && numberRadio.selected  )
   {
      numberTextBox.removeAttribute( "disabled"  );
   }
   else
   {
      numberTextBox.setAttribute( "disabled", "true" );
   }

   var untilRadio = document.getElementById( "repeat-until-radio" );
   var untilPicker = document.getElementById( "repeat-end-date-picker" );
   
   if( repeatCheckBox.checked && untilRadio.selected  )
   {
      untilPicker.removeAttribute( "disabled"  );
   }
   else
   {
      untilPicker.setAttribute( "disabled", "true" );
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
   var editTimeDisabled = allDayCheckBox.checked;
   
   var startTimePicker = document.getElementById( "start-datetime" );
   var endTimePicker = document.getElementById( "end-datetime" );

   startTimePicker.timepickerdisabled = editTimeDisabled;
   endTimePicker.timepickerdisabled = editTimeDisabled;
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
   var dayNumber = document.getElementById( "start-datetime" ).value.getDay();
   
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
   var dayNumber = document.getElementById( "start-datetime" ).value.getDay();
   
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
   var dayNumber = document.getElementById( "start-datetime" ).value.getDate();
   
   var dayExtension = getDayExtension( dayNumber );

   var weekNumber = getWeekNumberOfMonth();
   
   var calStrings = document.getElementById("bundle_calendar");

   var weekNumberText = getWeekNumberText( weekNumber );
   var dayOfWeekText = getDayOfWeek( dayNumber );
   var ofTheMonthText = document.getElementById( "ofthemonth-text" ).getAttribute( "value" );
   var lastText = document.getElementById( "last-text" ).getAttribute( "value" );
   var onTheText = document.getElementById( "onthe-text" ).getAttribute( "value" );
   var dayNumberWithOrdinal = dayNumber + dayExtension;
   var repeatSentence = calStrings.getFormattedString( "weekDayMonthLabel", [onTheText, dayNumberWithOrdinal, ofTheMonthText] );

   document.getElementById( "advanced-repeat-dayofmonth" ).setAttribute( "label", repeatSentence );
   
   if( weekNumber == 4 && isLastDayOfWeekOfMonth() )
   {
      document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [weekNumberText, dayOfWeekText, ofTheMonthText] ) );

      document.getElementById( "advanced-repeat-dayofweek-last" ).removeAttribute( "collapsed" );

      document.getElementById( "advanced-repeat-dayofweek-last" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [lastText, dayOfWeekText, ofTheMonthText] ) );
   }
   else if( weekNumber == 4 && !isLastDayOfWeekOfMonth() )
   {
      document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [weekNumberText, dayOfWeekText, ofTheMonthText] ) );

      document.getElementById( "advanced-repeat-dayofweek-last" ).setAttribute( "collapsed", "true" );
   }
   else
   {
      document.getElementById( "advanced-repeat-dayofweek" ).setAttribute( "label", calStrings.getFormattedString( "weekDayMonthLabel", [weekNumberText, dayOfWeekText, ofTheMonthText] ) );

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
      dateToAdd = document.getElementById( "exceptions-date-picker" ).value;
   }
   
   if( isAlreadyException( dateToAdd ) )
      return;

   var DateLabel = formatDate( dateToAdd );

   //add a row to the listbox.
   var listbox = document.getElementById( "exception-dates-listbox" );
   //ensure user can see that add occurred (also, avoid bug 231765, bug 250123)
   listbox.ensureElementIsVisible( listbox.appendItem( DateLabel, dateToAdd.getTime() ));

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
   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

   if ( dayNumber >= 1 && dayNumber <= 31 )
   {
      return( dateStringBundle.GetStringFromName( "ordinal.suffix."+dayNumber ) );
   }
   else
   {
      dump("ERROR: Invalid day number: " + dayNumber);
      return ( false );
   }
}

function getDayOfWeek( )
{
   //get the day number for today.
   var dayNumber = document.getElementById( "start-datetime" ).value.getDay();
   
   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

   //add one to the dayNumber because in the above prop. file, it starts at day1, but JS starts at 0
   var oneBasedDayNumber = parseInt( dayNumber ) + 1;
   
   return( dateStringBundle.GetStringFromName( "day."+oneBasedDayNumber+".name" ) );
   
}

function getWeekNumberOfMonth()
{
   //get the day number for today.
   var startTime = document.getElementById( "start-datetime" ).value;
   
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
   var startTime = document.getElementById( "start-datetime" ).value;
   
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
   var dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");
   if ( weekNumber >= 1 && weekNumber <= 4 )
   {
      return( dateStringBundle.GetStringFromName( "ordinal.name."+weekNumber) );
   }
   else if( weekNumber == 5 ) 
   {
       return( dateStringBundle.GetStringFromName( "ordinal.name.last" ) );
   }
   else
   {
      return( false );
   }
}

var launch = true;

/* URL */
function loadURL()
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
   launchBrowser( UrlToGoTo );

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
   
   if( newValue !== undefined ) {
      var field = document.getElementById( elementId );
      
      if( newValue === false ) {
         try {
             field.removeAttribute( propertyName );
         }
         catch (e) {
             dump("setFieldValue: field.removeAttribute couldn't remove "+propertyName+
                  " from "+elementId+" e: "+e+"\n");
         }
      }
      else {
         if( propertyName ) {
             try {
                 field.setAttribute( propertyName, newValue );
             }
             catch (e) {
                 dump("setFieldValue: field.setAttribute couldn't set "+propertyName+
                      " from "+elementId+" to "+newValue+" e: "+e+"\n");
             }
         }
         else {
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

function debug( text )
{
    if( debugenabled )
        dump( "\n"+ text + "\n");
}

// XXX lilmatt's new crap for the new XUL

function processAlarmType()
{
  var alarmMenu = document.getElementById("alarm-type");
  if( alarmMenu.selectedItem ) {
    dump("processAlarmType: "+alarmMenu.selectedItem.value+"\n");
    switch( alarmMenu.selectedItem.value )
    {
      case "none":
        hideElement("alarm-length-field");
        hideElement("alarm-length-units");
        hideElement("alarm-box-email");
        break;
      case "popup":
        showElement("alarm-length-field");
        showElement("alarm-length-units");
        hideElement("alarm-box-email");
        break;
      case "popupAndSound":
        showElement("alarm-length-field");
        showElement("alarm-length-units");
        hideElement("alarm-box-email");
        break;
      case "email":
        showElement("alarm-length-field");
        showElement("alarm-length-units");
        showElement("alarm-box-email");
        break;
    }
    // Make the window big enough for all the fields and widgets
    window.sizeToContent();
  }
  else
    dump("processAlarmType: no alarmMenu.selectedItem!\n");
}

function processComponentType()
{
  var componentMenu = document.getElementById("component-type");
  if( componentMenu.selectedItem) {
    dump("processComponentType: "+componentMenu.selectedItem.value+"\n");
    var args = window.arguments[0];
    switch( componentMenu.selectedItem.value )
    {
      case "ICAL_COMPONENT_EVENT":
        // Hide and show the appropriate fields and widgets
        hideElement("task-status-label");
        hideElement("cancelled-checkbox");
        showElement("event-status-label");
        showElement("event-status-field");
        showElement("all-day-event-checkbox");
        // Set menubar title correctly - New vs. Edit
        if( "new" == args.mode )
          titleDataItem = document.getElementById( "data-event-title-new" );
        else
          titleDataItem = document.getElementById( "data-event-title-edit" );
        document.getElementById("calendar-new-component-window").setAttribute("title", titleDataItem.getAttribute( "value" ));
        break;
      case "ICAL_COMPONENT_TODO":
        // Hide and show the appropriate fields and widgets
        hideElement("event-status-label");
        hideElement("event-status-field");
        hideElement("all-day-event-checkbox");
        showElement("task-status-label");
        showElement("cancelled-checkbox");
        // Set menubar title correctly - New vs. Edit
        if( "new" == args.mode )
          titleDataItem = document.getElementById( "data-event-title-new" );
        else
          titleDataItem = document.getElementById( "data-event-title-edit" );
        document.getElementById("calendar-new-component-window").setAttribute("title", titleDataItem.getAttribute( "value" ));
        break;
      //case "ICAL_COMPONENT_JOURNAL":
    }
    // Make the window big enough for all the fields and widgets
    window.sizeToContent();
  }
  else
    dump("processComponentType: no componentMenu.selectedItem!\n");
}
