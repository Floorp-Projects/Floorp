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
 *                 Chris Charabaruk <coldacid@meldstar.com>
 *                 Colin Phillips <colinp@oeone.com>
 *                 Karl Guertin <grayrest@grayrest.com> 
 *                 Mike Norton <xor@ivwnet.com>
 *                 ArentJan Banck <ajbanck@planet.nl> 
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

/** Code which generates event and task (todo) preview tooltips/titletips
    when the mouse hovers over either the event list, the task list, or
    an event or task box in one of the grid views. 

    (Portions of this code were previously in calendar.js and unifinder.js,
     some of it duplicated.)
**/

// Whether to show event details on mouseover (set to true or false by code)
var gShowTooltip = true;

/*
** A function to check if the tooltip should be shown.
*/
function checkTooltip( event )
{
   return( gShowTooltip );
}

/**
*  Called when a user hovers over a todo element and the text for the mouse over is changed.
*/

function getPreviewForTask( toDoItem )
{
  if( toDoItem )
  {
    gShowTooltip = true; //needed to show the tooltip.

    const vbox = document.createElement( "vbox" );
    boxInitializeHeaderGrid(vbox);

    var hasHeader = false;
         
    if (toDoItem.title)
    {
      boxAppendLabeledText(vbox, "tooltipTitle", toDoItem.title);
      hasHeader = true;
    }

    if (toDoItem.location)
    {
      boxAppendLabeledText(vbox, "tooltipLocation", toDoItem.location);
      hasHeader = true;
    }
   
    if (toDoItem.start && oeICalDateTime_isSet(toDoItem.start))
    {
      var startDate = new Date( toDoItem.start.getTime() );
      boxAppendLabeledDateTime(vbox, "tooltipStart", startDate, toDoItem.allDay);
      hasHeader = true;
    }
   
    if (toDoItem.due && oeICalDateTime_isSet(toDoItem.due))
    {
      var dueDate = new Date( toDoItem.due.getTime() );
      boxAppendLabeledDateTime(vbox, "tooltipDue", dueDate, toDoItem.allDay);
      hasHeader = true;
    }   

    if (toDoItem.priority && toDoItem.priority != 0)
    {
      boxAppendLabeledText(vbox, "tooltipPriority", String(toDoItem.priority));
      hasHeader = true;
    }

    if (toDoItem.status && toDoItem.status != toDoItem.ICAL_STATUS_NONE)
    {
      var status = getToDoStatusString(toDoItem);
      boxAppendLabeledText(vbox, "tooltipStatus", status);
      hasHeader = true;
    }

    if (toDoItem.percent && toDoItem.percent != 0)
    {
      boxAppendLabeledText(vbox, "tooltipPercent", String(toDoItem.percent)+"%");
      hasHeader = true;
    }

    if (toDoItem.completed && oeICalDateTime_isSet(toDoItem.completed))
    {
      var completedDate = new Date( toDoItem.completed.getTime() );
      boxAppendLabeledDateTime(vbox, "tooltipCompleted", completedDate, toDoItem.allDay);
      hasHeader = true;
    }


    if (toDoItem.description)
    {
      // display up to 4 description lines like body of message below headers
      if (hasHeader)
        boxAppendText(vbox, ""); 

      boxAppendLines(vbox, toDoItem.description, 4);
    }
      
    return ( vbox );
  } 
  else
  {
    gShowTooltip = false; //Don't show the tooltip
    return null;
  }
}

/**
*  Called when mouse moves over a different event display box.
*  An ICalEventDisplay represents an instance of a possibly recurring event.
*/
function getPreviewForEventDisplay( calendarEventDisplay )
{
  var instStartDate = new Date(calendarEventDisplay.displayDate);
  // End date is normally at the time at which event ends.
  // So AllDay displayed ending today end at 0:00 next day.
  var instEndDate = new Date(calendarEventDisplay.displayEndDate);
  if (calendarEventDisplay.event.allDay)
    instEndDate.setDate(instEndDate.getDate() + 1);

  return getPreviewForEvent(calendarEventDisplay.event,
                            instStartDate, instEndDate);
}

/**
*  Called when mouse moves over a different, or
*  when mouse moves over event in event list.
*  The instStartDate is date of instance displayed at event box
*  (recurring or multiday events may be displayed by more than one event box
*  for different days), or null if should compute next instance from now.
*/
function getPreviewForEvent( event, instStartDate, instEndDate )
{
  const vbox = document.createElement( "vbox" );
  boxInitializeHeaderGrid(vbox);
    
  if (event)
  {
    gShowTooltip = true;

    if (event.title)
    {
      boxAppendLabeledText(vbox, "tooltipTitle", event.title);
    }

    if (event.location)
    {
      boxAppendLabeledText(vbox, "tooltipLocation", event.location);
    }

    if (event.start || instStartDate)
    {
      var eventStart = new Date(event.start.getTime());
      var eventEnd = event.end && new Date(event.end.getTime());
      if (event.allDay && eventEnd) { 
        eventEnd.setDate(eventEnd.getDate() - 1);
      }
      var relativeToDate;

      if (!eventEnd ||
          gCalendarWindow.dateFormater.isOnSameDate(eventStart, eventEnd)) {
        // event within single day, ok to omit dates if same as relativeToDate
        relativeToDate = instStartDate || new Date(); // today
      } else {
        // event spanning multiple days, do not omit dates.
        // For multiday events use event start/end, not grid start/end.
        relativeToDate = false;
        instStartDate = null;
        instEndDate = null;
      }

      var startDate, endDate;
      if (instStartDate && instEndDate) {
        startDate = instStartDate;
        endDate = instEndDate;
      } else {
        // Event may be recurrent event.   If no displayed instance specified,
        // use next instance, or previous instance if no next instance.
        startDate = instStartDate || getCurrentNextOrPreviousRecurrence(event);
        var eventDuration = (event.end
                             ? event.end.getTime() - event.start.getTime()
                             : 0);
        endDate = new Date(startDate.getTime() + eventDuration);
      }
      boxAppendLabeledDateTimeInterval(vbox, "tooltipDate",
                                       startDate, endDate, event.allDay,
                                       relativeToDate);
    }

    if (event.status && event.status != event.ICAL_STATUS_NONE)
    {
      var statusString = getEventStatusString(event);
      boxAppendLabeledText(vbox, "tooltipStatus", statusString);
    }

    if (event.description)
    {
      // display up to 4 description lines, like body of message below headers
      boxAppendText(vbox, ""); 
      boxAppendLines(vbox, event.description, 4);
    }

    return ( vbox );
  }
  else
  {
    gShowTooltip = false; //Don't show the tooltip
    return null;
  }
}


/** String for event status: (none), Tentative, Confirmed, or Cancelled **/
function getEventStatusString(calendarEvent)
{
  switch( calendarEvent.status )
  {
     case calendarEvent.ICAL_STATUS_TENTATIVE:
        return( gCalendarBundle.getString( "statusTentative" ) );
     case calendarEvent.ICAL_STATUS_CONFIRMED:
        return( gCalendarBundle.getString( "statusConfirmed" ) );
     case calendarEvent.ICAL_STATUS_CANCELLED:
        return( gCalendarBundle.getString( "statusCancelled" ) );
     case calendarEvent.ICAL_STATUS_NONE:
     default: 
        return "";
  }
}

/** String for todo status: (none), NeedsAction, InProcess, Cancelled, or Completed **/
function getToDoStatusString(iCalToDo)
{
  switch( iCalToDo.status )
  {
     case iCalToDo.ICAL_STATUS_NEEDSACTION:
        return( gCalendarBundle.getString( "statusNeedsAction" ) );
     case iCalToDo.ICAL_STATUS_INPROCESS:
        return( gCalendarBundle.getString( "statusInProcess" ) );
     case iCalToDo.ICAL_STATUS_CANCELLED:
        return( gCalendarBundle.getString( "statusCancelled" ) );
     case iCalToDo.ICAL_STATUS_COMPLETED:
        return( gCalendarBundle.getString( "statusCompleted" ) );
     case iCalToDo.ICAL_STATUS_NONE:
     default: 
        return "";
  }
}

/** PRIVATE: Append 1 line of text to box inside an xul description node containing a single text node **/
function boxAppendText(box, textString)
{
  var textNode = document.createTextNode(textString);
  var xulDescription = document.createElement("description");
  xulDescription.setAttribute("class", "tooltipBody");
  xulDescription.appendChild(textNode);
  box.appendChild(xulDescription);
}
/** PRIVATE: Append multiple lines of text to box, each line inside an xul description node containing a single text node **/
function boxAppendLines(vbox, textString, maxLineCount)
{
  if (!maxLineCount)
    maxLineCount = 4;

  // trim trailing whitespace
  var end = textString.length;
  for (; end > 0; end--)
    if (" \t\r\n".indexOf(textString.charAt(end - 1)) == -1)
      break;
  textString = textString.substring(0, end);

  var lines = textString.split("\n");
  var lineCount = lines.length;
  if( lineCount > maxLineCount ) {
    lineCount = maxLineCount;
    lines[ maxLineCount ] = "..." ;
  }

  for (var i = 0; i < lineCount; i++) {
    boxAppendText(vbox, lines[i]);
  }
}
/** PRIVATE: Use dateFormater to format date and time.
    Append date to box inside an xul description node containing a single text node. **/
function boxAppendLabeledDateTime(box, labelProperty, date, isAllDay )
{
  var jsDate = new Date(date.getTime());
  var formattedDateTime = gCalendarWindow.dateFormater.formatDateTime( jsDate, true, isAllDay );
  boxAppendLabeledText(box, labelProperty, formattedDateTime);
}
/** PRIVATE: Use dateFormater to format date and time interval.
    Append interval to box inside an xul description node containing a single text node. **/
function boxAppendLabeledDateTimeInterval(box, labelProperty, start, end, isAllDay, relativeToDate)
{
  var formattedInterval = gCalendarWindow.dateFormater.formatInterval( start, end, isAllDay, relativeToDate);
  boxAppendLabeledText(box, labelProperty, formattedInterval);
}

function boxInitializeHeaderGrid(box)
{
  var grid = document.createElement("grid");
  var rows;
  {
    var columns = document.createElement("columns");
    {
      columns.appendChild(document.createElement("column"));
      columns.appendChild(document.createElement("column"));
    }
    grid.appendChild(columns);
    rows = document.createElement("rows");
    grid.appendChild(rows);
  }
  box.appendChild(grid);
}

function boxAppendLabeledText(box, labelProperty, textString)
{
  var labelText = gCalendarBundle.getString(labelProperty);
  var rows = box.getElementsByTagName("rows")[0];
  { 
    var row = document.createElement("row");
    {
      row.appendChild(createTooltipHeaderLabel(labelText));
      row.appendChild(createTooltipHeaderDescription(textString));
    }
    rows.appendChild(row);
  }
}

function createTooltipHeaderLabel(text)
{
  var label = document.createElement("label");
  label.setAttribute("class", "tooltipHeaderLabel");
  label.appendChild(document.createTextNode(text));
  return label;
}

function createTooltipHeaderDescription(text)
{
  var label = document.createElement("description");
  label.setAttribute("class", "tooltipHeaderDescription");
  label.appendChild(document.createTextNode(text));
  return label;
}
