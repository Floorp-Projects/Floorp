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


var gToDo;          // event being edited
var gOnOkFunction;   // function to be called when user clicks OK

var gTimeDifference = 3600000;  //when editing an event, we change the end time if the start time is changing. This is the difference for the event time.
var gDefaultAlarmLength = 15; //number of minutes to default the alarm to

var gMode = ''; //what mode are we in? new or edit...

/*-----------------------------------------------------------------
*   W I N D O W      F U N C T I O N S
*/

/**
*   Called when the dialog is loaded.
*/

function loadCalendarToDoDialog()
{
   // Get arguments, see description at top of file
   
   var args = window.arguments[0];
   
   gMode = args.mode;
   
   gOnOkFunction = args.onOk;
   gToDo = args.calendarToDo;
   
   // mode is "new or "edit" - show proper header
   var titleDataItem = null;

   if( "new" == args.mode )
   {
      titleDataItem = document.getElementById( "data-todo-title-new" );
   }
   else
   {
      titleDataItem = document.getElementById( "data-todo-title-edit" );
   }
   
   var titleString = titleDataItem.getAttribute( "value" );
   document.getElementById("calendar-new-taskwindow").setAttribute("title", titleString);

   // fill in fields from the event
   var dueDate = new Date( gToDo.due.getTime() );
   document.getElementById( "due-date-picker" ).value = dueDate;

   var startDate = new Date( gToDo.start.getTime() );
   document.getElementById( "start-date-picker" ).value = startDate;

   setFieldValue( "priority-levels", gToDo.priority );
   
   setFieldValue( "percent-complete-menulist", gToDo.percent );

   if( gToDo.completed.getTime() > 0 )
   {
      var completedDate = new Date( gToDo.completed.getTime() );
      document.getElementById( "completed-date-picker" ).value = completedDate; 
      
      setFieldValue( "completed-checkbox", "true", "checked" );
   }
   else
   {
      var Today = new Date();
      document.getElementById( "completed-date-picker" ).value = Today;
   }

   setFieldValue( "title-field", gToDo.title  );
   setFieldValue( "description-field", gToDo.description );
   setFieldValue( "uri-field", gToDo.url );
   
   switch( gToDo.status )
   {
      case gToDo.ICAL_STATUS_CANCELLED:
         setFieldValue( "cancelled-checkbox", true, "checked" );
      break;
   }
   
   setFieldValue( "private-checkbox", gToDo.privateEvent, "checked" );
   
   if( gToDo.alarm === false && gToDo.alarmLength == 0 )
   {
      gToDo.alarmLength = gDefaultAlarmLength;
   }
   
   setFieldValue( "alarm-checkbox", gToDo.alarm, "checked" );
   setFieldValue( "alarm-length-field", gToDo.alarmLength );
   setFieldValue( "alarm-length-units", gToDo.alarmUnits );


   // Load categories
   var categoriesString = opener.getCharPref(opener.gCalendarWindow.calendarPreferences.calendarPref, "categories.names", getDefaultCategories() );
   var categoriesList = categoriesString.split( "," );
   
   // insert the category already in the task so it doesn't get lost
   if( gToDo.categories )
      if( categoriesString.indexOf( gToDo.categories ) == -1 )
         categoriesList[categoriesList.length] =  gToDo.categories;

   categoriesList.sort();
   
   var oldMenulist = document.getElementById( "categories-menulist-menupopup" );
   while( oldMenulist.hasChildNodes() )
      oldMenulist.removeChild( oldMenulist.lastChild );
   
   for (var i = 0; i < categoriesList.length ; i++)
   {
      document.getElementById( "categories-field" ).appendItem(categoriesList[i], categoriesList[i]);
   }

   document.getElementById( "categories-field" ).selectedIndex = -1;
   setFieldValue( "categories-field", gToDo.categories );
   
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
      setFieldValue( "server-field", gToDo.parent.server );

      //for now you can't edit which file the event is in.
      setFieldValue( "server-field", "true", "disabled" );

      setFieldValue( "server-field-label", "true", "disabled" );
   }
   
   //the next line seems to crash Mozilla
   //setFieldValue( "server-field", gEvent.parent.server );
   
   // update enabling and disabling
   updateAlarmItemEnabled();
   updateCompletedItemEnabled();
   
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
   
   gToDo.title       = getFieldValue( "title-field" );
   gToDo.description = getFieldValue( "description-field" );
   
   var dueDate = document.getElementById( "due-date-picker" ).value;
   gToDo.due.year = dueDate.getYear()+1900;
   gToDo.due.month = dueDate.getMonth();
   gToDo.due.day = dueDate.getDate();
   gToDo.due.hour = 23;
   gToDo.due.minute = 59;
   
   var startDate = document.getElementById( "start-date-picker" ).value;
   gToDo.start.year = startDate.getYear()+1900;
   gToDo.start.month = startDate.getMonth();
   gToDo.start.day = startDate.getDate();
   gToDo.start.hour = 0;
   gToDo.start.minute = 0;
   
   gToDo.url = getFieldValue( "uri-field" );

   gToDo.privateEvent = getFieldValue( "private-checkbox", "checked" );
   
   gToDo.alarm       = getFieldValue( "alarm-checkbox", "checked" );
   gToDo.alarmLength = getFieldValue( "alarm-length-field" );
   gToDo.alarmUnits  = getFieldValue( "alarm-length-units", "value" );  

   gToDo.priority    = getFieldValue( "priority-levels", "value" );
   var completed = getFieldValue( "completed-checkbox", "checked" );

   gToDo.categories  = getFieldValue( "categories-field", "value" );

   var percentcomplete = getFieldValue( "percent-complete-menulist" );
   percentcomplete =  parseInt( percentcomplete );
   
   if(percentcomplete > 100)
      percentcomplete = 100;
   else if(percentcomplete < 0)
      percentcomplete = 0;
      
   gToDo.percent = percentcomplete;
   
   if( completed )
   {
      //get the time for the completed event
      var completedDate = document.getElementById( "completed-date-picker" ).value;

      gToDo.completed.year = completedDate.getYear() + 1900;
      gToDo.completed.month = completedDate.getMonth();
      gToDo.completed.day = completedDate.getDate();
      gToDo.status = gToDo.ICAL_STATUS_COMPLETED;
   }
   else
   {
      gToDo.completed.clear();
      
      var cancelled = getFieldValue( "cancelled-checkbox", "checked" );
      
      if( cancelled )
         gToDo.status = gToDo.ICAL_STATUS_CANCELLED;
      else if (percentcomplete > 0)
         gToDo.status = gToDo.ICAL_STATUS_INPROCESS;
      else
         gToDo.status = gToDo.ICAL_STATUS_NEEDSACTION;
   }
   
   
   if ( getFieldValue( "alarm-email-checkbox", "checked" ) ) 
   {
      gToDo.alarmEmailAddress = getFieldValue( "alarm-email-field", "value" );
   }
   else
   {
      gToDo.alarmEmailAddress = "";
   }
   
   var Server = getFieldValue( "server-field" );
   
   // :TODO: REALLY only do this if the alarm or start settings change.?
   //if the end time is later than the start time... alert the user using text from the dtd.
   // call caller's on OK function
   gOnOkFunction( gToDo, Server );
      
   // tell standard dialog stuff to close the dialog
   return true;
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

   checkStartAndDueDates();
}

function checkStartAndDueDates()
{
   var StartDate = document.getElementById( "start-date-picker" ).value;

   var dueDate = document.getElementById( "due-date-picker" ).value;
      
   if( DueDate.getTime() < StartDate.getTime() )
   {
      //show alert message, disable OK button
      document.getElementById( "start-date-warning" ).removeAttribute( "collapsed" );

      document.getElementById( "calendar-new-taskwindow" ).getButton( "accept" ).setAttribute( "disabled", true );
   }
   else
   {
      //enable OK button
      
      document.getElementById( "start-date-warning" ).setAttribute( "collapsed", true );
      
      document.getElementById( "calendar-new-taskwindow" ).getButton( "accept" ).removeAttribute( "disabled" );
   }

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

function updateCompletedItemEnabled()
{
   var completedCheckbox = "completed-checkbox";

   if( getFieldValue( completedCheckbox, "checked" ) )
   {
      setFieldValue( "completed-date-picker", false, "disabled" );
      setFieldValue( "percent-complete-menulist", "100" );
      setFieldValue( "percent-complete-menulist", true, "disabled" );
      setFieldValue( "percent-complete-text", true, "disabled" );
   }
   else
   {
      setFieldValue( "completed-date-picker", true, "disabled" );
      setFieldValue( "percent-complete-menulist", false, "disabled" );
      setFieldValue( "percent-complete-text", false, "disabled" );
      if( getFieldValue( "percent-complete-menulist" ) == 100 )
         setFieldValue( "percent-complete-menulist", "0" );
   }
}

function percentCompleteCommand()
{
   var percentcompletemenu = "percent-complete-menulist";
   var percentcomplete = getFieldValue( "percent-complete-menulist" );
   percentcomplete =  parseInt( percentcomplete );
   if( percentcomplete == 100)
      setFieldValue( "completed-checkbox", "true", "checked" );
     
   updateCompletedItemEnabled();
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
*   Handle key down in alarm field
*/

function alarmLengthKeyDown( repeatField )
{
    updateAlarmPlural();
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
      
      if( !field )
         alert( elementId+" not found" );

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
