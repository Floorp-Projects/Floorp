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
 *                 Chris Allen
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



/***** calendar/printDialog.js
* PRIMARY AUTHOR
*   Chris Allen
* REQUIRED INCLUDES 
*   <script type="application/x-javascript" src="chrome://calendar/content/dateUtils.js"/>
*   
* NOTES
*   Code for the calendar's print dialog.
*
*  Invoke this dialog to print a Calendar as follows:
*  args = new Object();
*  args.eventSource = youreventsource;
*  args.selectedEvents=currently selected events
*  args.selectedDate=currently selected date
*  args.weeksInView=multiweek how many weeks to show
*  args.prevWeeksInView=previous weeks to show in view
*  args.startOfWeek=zero based day to start the week
*  calendar.openDialog("chrome://calendar/content/eventDialog.xul", "printdialog", "chrome,modal", args );
*
* IMPLEMENTATION NOTES
**********
*/

/*-----------------------------------------------------------------
 *   W I N D O W      V A R I A B L E S
 */

var selectedEvents; // selected events send by opener
var selectedDate; // current selected date sent by opener

var gCategoryManager; // for future
gCalendarWindow = window.opener.gCalendarWindow ;
var gStartDate = new Date( );

var gArgs;
var gPrintSettings = null;

/*-----------------------------------------------------------------
 *   W I N D O W      F U N C T I O N S
 */

/*-----------------------------------------------------------------
 *   Called when the dialog is loaded.
 */

function loadCalendarPrintDialog()
{
  // load up the sent arguments.

  gArgs = window.arguments[0];
  gStartDate = gArgs.selectedDate ;
  gSelectedEvents = gArgs.selectedEvents ;
  // set the date to the currently selected date
  document.getElementById( "start-date-picker" ).value = selectedDate;

  // start focus on title
  var firstFocus = document.getElementById( "title-field" );
  firstFocus.focus();

  opener.setCursor( "default" );
}


function printCalendar() {
  var caltype=document.getElementById("view-field");
  if (caltype.value == '')
    caltype.value='month';

  var printFunction;
  var printFunctionArg = gStartDate ;

  switch( caltype.value )
  {
    case 'month' :
      printFunction = "printMonthView" ;
      break ;
    case 'list' :
      printFunction = "printEventArray" ;
      printFunctionArg = gSelectedEvents ;
      break ;
    case 'day' :
      printFunction = "printDayView" ;
      break ;
    case 'week' :
      printFunction = "printWeekView" ;
      break ;
    case 'multiweek' :
      printFunction = "printMultiWeekView" ;
      break ;
    default :
      alert("Error : no case in printDialog.js::printCalendar()");
      return false ;
  }
  printInitWindow(printFunction,printFunctionArg)
  return true ;
}

function printInitWindow(printFunction,printFunctionArg)
{
  var mytitle=document.getElementById("title-field").value;
  var showprivate=document.getElementById("private-checkbox");

  printwindow=window.openDialog("chrome://calendar/content/calPrintEngine.xul",
				"CalendarPrintWindow",
				"chrome,dialog=no,all,centerscreen",
				printFunction,printFunctionArg,mytitle,showprivate,gArgs,gCalendarWindow );
  // to set the title of the page
  printwindow.title = printwindow.name ; 
  return printwindow ;
}

/*-----------------------------------------------------------------
 *   Called when a datepicker is finished, and a date was picked.
 */

function onDatePick( datepicker )
{
  var ThisDate = new Date( datepicker.value);

  if( datepicker.id == "start-date-picker" )
  {
    gStartDate.setMonth( ThisDate.getMonth() );
    gStartDate.setDate( ThisDate.getDate() );
    gStartDate.setFullYear( ThisDate.getFullYear() );
  }

}

/*-----------------------------------------------------------------
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
    } else {
      if( propertyName )
      {
        field.setAttribute( propertyName, newValue );
      } else {
        field.value = newValue;
      }
    }
  }
}

/*-----------------------------------------------------------------
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
  } else {
    return field.value;
  }
}

/*-----------------------------------------------------------------
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

/*-----------------------------------------------------------------
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

/*-----------------------------------------------------------------
 *   Take a Date object and return a displayable date string i.e.: May 5, 1959
 *  :TODO: This should be moved into DateFormater and made to use some kind of
 *         locale or user date format preference.
 */

function formatDate( date )
{
  return( opener.gCalendarWindow.dateFormater.getFormatedDate( date ) );
}

//Needed?
function debug( Text )
{
  dump( "\nprintDialog.js:"+ Text + "\n");
}
