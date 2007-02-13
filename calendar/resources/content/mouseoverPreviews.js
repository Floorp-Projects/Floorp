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

/** PUBLIC
*
*  This changes the mouseover preview based on the start and end dates
*  of an occurrence of a (one-time or recurring) calEvent or calToDo.
*  Used by all grid views.
*/

function onMouseOverItem( occurrenceBoxMouseEvent )
{
  if ("occurrence" in occurrenceBoxMouseEvent.currentTarget) {
    // occurrence of repeating event or todo
    var occurrence = occurrenceBoxMouseEvent.currentTarget.occurrence;

    const toolTip = document.getElementById("itemTooltip");

    var holderBox;
    if (isEvent(occurrence)) {
      holderBox = getPreviewForEvent(occurrence, occurrence.startDate, occurrence.endDate);
    } else if (isToDo(occurrence)) {
      holderBox = getPreviewForTask(occurrence);
    }
    if (holderBox) {
      setToolTipContent(toolTip, holderBox);
      return true;
    } 
  }
  return false;
}

/** For all instances of an event, as displayed by unifinder. **/
function onMouseOverEventTree( toolTip, mouseEvent )
{
  var item = getCalendarEventFromEvent( mouseEvent );
  if (isEvent(item)) {
    var holderBox = getPreviewForEvent(item);
    if (holderBox) {
      setToolTipContent(toolTip, holderBox);
      return true;
    } 
  }
  return false;
}

/** For all instances of a task, as displayed by unifinderToDo. **/
function onMouseOverTaskTree( toolTip, mouseEvent )
{
  var item = getToDoFromEvent( mouseEvent );
  if (isToDo(item)) {
    var holderBox = getPreviewForTask(item);
    if (holderBox) {
      setToolTipContent(toolTip, holderBox);
      return true;
    }
  }
  return false;
}

/*
** add newContentBox,
*/
function setToolTipContent(toolTip, holderBox)
{
  while (toolTip.hasChildNodes())
    toolTip.removeChild( toolTip.firstChild );
  toolTip.appendChild( holderBox );
}

/**
*  Called when a user hovers over a todo element and the text for the mouse over is changed.
*/

function getPreviewForTask( toDoItem )
{
  if( toDoItem )
  {
    const vbox = document.createElement( "vbox" );
    boxInitializeHeaderGrid(vbox);

    var hasHeader = false;
         
    if (toDoItem.title)
    {
      boxAppendLabeledText(vbox, "tooltipTitle", toDoItem.title);
      hasHeader = true;
    }

    var location = toDoItem.getProperty("LOCATION");
    if (location)
    {
      boxAppendLabeledText(vbox, "tooltipLocation", location);
      hasHeader = true;
    }
   
    if (toDoItem.entryDate && toDoItem.entryDate.isValid)
    {
      boxAppendLabeledDateTime(vbox, "tooltipStart", toDoItem.entryDate);
      hasHeader = true;
    }
   
    if (toDoItem.dueDate && toDoItem.dueDate.isValid)
    {
      boxAppendLabeledDateTime(vbox, "tooltipDue", toDoItem.dueDate);
      hasHeader = true;
    }   

    if (toDoItem.priority && toDoItem.priority != 0)
    {
      var priorityInteger = parseInt(toDoItem.priority);
      var priorityString;

      // These cut-offs should match calendar-event-dialog.js
      if (priorityInteger >= 1 && priorityInteger <= 4) {
           priorityString = calGetString('calendar', 'highPriority'); // high priority
      } else if (priorityInteger == 5) {
          priorityString = calGetString('calendar', 'mediumPriority'); // medium priority
      } else {
          priorityString = calGetString('calendar', 'lowPriority'); // low priority
      }
      boxAppendLabeledText(vbox, "tooltipPriority", priorityString);
      hasHeader = true;
    }

    if (toDoItem.status && toDoItem.status != "NONE")
    {
      var status = getToDoStatusString(toDoItem);
      boxAppendLabeledText(vbox, "tooltipStatus", status);
      hasHeader = true;
    }

    if (toDoItem.percentComplete != 0 && toDoItem.percentComplete != 100)
    {
      boxAppendLabeledText(vbox, "tooltipPercent", String(toDoItem.percentComplete)+"%");
      hasHeader = true;
    } else if (toDoItem.percentComplete == 100)
    {
      if (toDoItem.completedDate == null) {
        boxAppendLabeledText(vbox, "tooltipPercent", "100%");
      } else { 
        boxAppendLabeledDateTime(vbox, "tooltipCompleted", toDoItem.completedDate);
      } 
      hasHeader = true;
    }

    var description = toDoItem.getProperty("DESCRIPTION");
    if (description)
    {
      // display up to 4 description lines like body of message below headers
      if (hasHeader)
        boxAppendText(vbox, ""); 

      boxAppendLines(vbox, description, 4);
    }
      
    return ( vbox );
  } 
  else
  {
    return null;
  }
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
    if (event.title)
    {
      boxAppendLabeledText(vbox, "tooltipTitle", event.title);
    }

    var location = event.getProperty("LOCATION");
    if (location)
    {
      boxAppendLabeledText(vbox, "tooltipLocation", location);
    }

    if (event.startDate || instStartDate)
    {
      var startDate, endDate;
      if (instStartDate && instEndDate) {
        startDate = instStartDate;
        endDate = instEndDate;
      } else {
        // Event may be recurrent event.   If no displayed instance specified,
        // use next instance, or previous instance if no next instance.
        var occ = getCurrentNextOrPreviousRecurrence(event);
        startDate = instStartDate || occ.startDate;
        endDate = occ.endDate;
      }
      boxAppendLabeledDateTimeInterval(vbox, "tooltipDate", startDate, endDate);
    }

    if (event.status && event.status != "NONE")
    {
      var statusString = getEventStatusString(event);
      boxAppendLabeledText(vbox, "tooltipStatus", statusString);
    }

    var description = event.getProperty("DESCRIPTION");
    if (description)
    {
      // display up to 4 description lines, like body of message below headers
      boxAppendText(vbox, ""); 
      boxAppendLines(vbox, description, 4);
    }

    return ( vbox );
  }
  else
  {
    return null;
  }
}


/** String for event status: (none), Tentative, Confirmed, or Cancelled **/
function getEventStatusString(calendarEvent)
{
  switch( calendarEvent.status )
  {
    // Event status value keywords are specified in RFC2445sec4.8.1.11
    case "TENTATIVE":
      return calGetString('calendar', "statusTentative");
    case "CONFIRMED":
      return calGetString('calendar', "statusConfirmed");
    case "CANCELLED":
      return calGetString('calendar', "statusCancelled");
     default: 
        return "";
  }
}

/** String for todo status: (none), NeedsAction, InProcess, Cancelled, or Completed **/
function getToDoStatusString(iCalToDo)
{
  switch( iCalToDo.status )
  {
    // Todo status keywords are specified in RFC2445sec4.8.1.11
    case "NEEDS-ACTION":
      return calGetString('calendar', "statusNeedsAction");
    case "IN-PROCESS":
      return calGetString('calendar', "statusInProcess");
    case "CANCELLED":
      return calGetString('calendar', "statusCancelled");
    case "COMPLETED":
      return calGetString('calendar', "statusCompleted");
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
/** PRIVATE: Use dateFormatter to format date and time.
    Append date to box inside an xul description node containing a single text node. **/
function boxAppendLabeledDateTime(box, labelProperty, date)
{
  var dateFormatter = Components.classes["@mozilla.org/calendar/datetime-formatter;1"]
                                .getService(Components.interfaces.calIDateTimeFormatter);
  date = date.getInTimezone(calendarDefaultTimezone());
  var formattedDateTime = dateFormatter.formatDateTime(date);
  boxAppendLabeledText(box, labelProperty, formattedDateTime);
}
/** PRIVATE: Use dateFormatter to format date and time interval.
    Append interval to box inside an xul description node containing a single text node. **/
function boxAppendLabeledDateTimeInterval(box, labelProperty, start, end)
{
  var dateFormatter = Components.classes["@mozilla.org/calendar/datetime-formatter;1"]
                                .getService(Components.interfaces.calIDateTimeFormatter);
  var startString = new Object();
  var endString = new Object();
  start = start.getInTimezone(calendarDefaultTimezone());
  end = end.getInTimezone(calendarDefaultTimezone());
  dateFormatter.formatInterval(start, end, startString, endString);
  if (endString.value != "") {
    boxAppendLabeledText(box, labelProperty, startString.value + ' - ' + endString.value);
  } else {
    boxAppendLabeledText(box, labelProperty, startString.value);
  }
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
  var labelText = calGetString('calendar', labelProperty);
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

/** If now is during an occurrence, return the ocurrence.
    Else if now is before an ocurrence, return the next ocurrence.
    Otherwise return the previous ocurrence. **/
function getCurrentNextOrPreviousRecurrence(calendarEvent)
{
    if (!calendarEvent.recurrenceInfo) {
        return calendarEvent;
    }

    var dur = calendarEvent.duration.clone();
    dur.isNegative = true;

    // To find current event when now is during event, look for occurrence
    // starting duration ago.
    var probeTime = now();
    probeTime.addDuration(dur);

    var occ = calendarEvent.recurrenceInfo.getNextOccurrence(probeTime);

    if (!occ) {
        var occs = calendarEvent.recurrenceInfo.getOccurrences(calendarEvent.startDate, probeTime, 0, {});
        occ = occs[occs.length -1];
    }
    return occ;
}

