/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
 *   Joey Minta <jminta@gmail.com>
 *   Michael Buettner <michael.buettner@sun.com>
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

//////////////////////////////////////////////////////////////////////////////
// onLoad
//////////////////////////////////////////////////////////////////////////////

function onLoad()
{
  var args = window.arguments[0];

  window.onAcceptCallback = args.onOk;
  window.calendarItem = args.calendarEvent;
  window.accept = [ true,true ];

  document.getElementById("sun-calendar-event-dialog").getButton("accept").setAttribute("collapsed","true");
  document.getElementById("sun-calendar-event-dialog").getButton("cancel").setAttribute("collapsed","true");
  document.getElementById("sun-calendar-event-dialog").getButton("cancel").parentNode.setAttribute("collapsed","true");

  // the different tabpages occasionally send an 'accept'-event
  // to indicate whether or not the accept-button should be
  // enabled or disabled. this is the handler that involves
  // the necessary steps in order to reflect the new state.
  var accept = function acceptHandler(event) {
  
    // the bool indicating the new state is expected to be stored in 'event.details'
    window.accept[event.tab] = event.details;
    var enableAccept = true;
    for each(var accept in window.accept) {
      if (!accept)
        enableAccept = false;
    }
    
    // we need to modify the accept-button, get the reference to the button
    var acceptButton = document.getElementById("button-save");
    
    // set or remove the 'disabled' attribute depending on the requested state.
    if (!enableAccept) {
      acceptButton.setAttribute("disabled", "true");
    } else if (acceptButton.getAttribute("disabled")) {
      acceptButton.removeAttribute("disabled");
    }
  };
  
  // attach the above defined handler to the 'accept'-event.
  window.addEventListener('accept', accept, true);

  var warning = function warningHandler(event) {
    var statusbarPanel = document.getElementById("statusbarpanel");
    if(event.details) {
      statusbarPanel.setAttribute("label",event.details);
    } else {
      statusbarPanel.removeAttribute("label");
    }
  };
  window.addEventListener('warning', warning, true);

  var calendar = function calendarHandler(event) {
    var recurrencepage = document.getElementById("recurrence-page");
    recurrencepage.onChangeCalendar(event.details);
    var attendeespage = document.getElementById("attendees-page");
    attendeespage.onChangeCalendar(event.details);
  };
  window.addEventListener('calendar', calendar, true);

  // figure out what the title of the dialog should be and set it
  updateTitle();

  if (isToDo(window.calendarItem))
    document.getElementById("attendees-tab").setAttribute("collapsed","true");

  opener.setCursor("auto");

  self.focus();
}

//////////////////////////////////////////////////////////////////////////////
// onTabSelected
//////////////////////////////////////////////////////////////////////////////

function onTabSelected()
{
  var statusbarPanel = document.getElementById("statusbarpanel");
  statusbarPanel.removeAttribute("label");
  
  var tab = document.getElementById("dialog-tab");
  var index = tab.selectedIndex;
  var attendees = document.getElementById("attendees-page");
  var main = document.getElementById("main-page");

  if(index == 0) {
    if(attendees.startDate) {
      setElementValue("event-starttime",attendees.startDate.jsDate);
      setElementValue("event-endtime",attendees.endDate.jsDate);
    }
  } else if(index == 1) {
    // disable/enable recurrence options based on whether or not
    // we're editing a task and whether or not this task has
    // an associated startdate.
    var item = window.calendarItem;
    if (isToDo(item)) {
      var entryDate = getElementValue("todo-has-entrydate", "checked") ? 
                      jsDateToDateTime(getElementValue("todo-entrydate")) : null;
      if(entryDate) {
        var kDefaultTimezone = calendarDefaultTimezone();
        entryDate = entryDate.getInTimezone(kDefaultTimezone);
      }
      var recurrencepage = document.getElementById("recurrence-page");
      recurrencepage.mStartDate = entryDate;
      recurrencepage.disableOrEnable(item);
      recurrencepage.updateRecurrenceControls();
    }
  } else if(index == 2) {
    var kDefaultTimezone = calendarDefaultTimezone();
    var startDate = jsDateToDateTime(getElementValue("event-starttime"));
    var endDate = jsDateToDateTime(getElementValue("event-endtime"));
    startDate = startDate.getInTimezone(kDefaultTimezone);
    endDate = endDate.getInTimezone(kDefaultTimezone);
    var isAllDay = getElementValue("event-all-day", "checked");
    if (isAllDay) {
        startDate.isDate = true;
        startDate.normalize();
        endDate.isDate = true;
        endDate.day += 1;
        endDate.normalize();
    }
    attendees.setTimeRange(startDate,endDate);
  }
}

//////////////////////////////////////////////////////////////////////////////
// onAccept
//////////////////////////////////////////////////////////////////////////////

function onAccept()
{
  // the start/enddate is unfortunately duplicated [once on the
  // mainpage and once more on the attendees-page]. that's why
  // we need to check if we're called while being on the attendees-page.
  // in this case we manually transfer the start/enddate to the appropriate
  // controls on the mainpage.
  var tab = document.getElementById("dialog-tab");
  if(tab.selectedIndex == 2) {
    var attendees = document.getElementById("attendees-page");
    if(attendees.startDate) {
      setElementValue("event-starttime",   attendees.startDate.jsDate);
      setElementValue("event-endtime",     attendees.endDate.jsDate);
    }
  }

  // if this event isn't mutable, we need to clone it like a sheep
  var originalItem = window.calendarItem;
  var item = (originalItem.isMutable) ? originalItem : originalItem.clone();

  var mainpage = document.getElementById("main-page");
  mainpage.onSave(item);
  var recurrencepage = document.getElementById("recurrence-page");
  item.recurrenceInfo = recurrencepage.onSave(item);
  var attendeespage = document.getElementById("attendees-page");
  attendeespage.onSave(item);
  dump(item.icalString + "\n");

  var calendar = document.getElementById("item-calendar").selectedItem.calendar;
  window.onAcceptCallback(item, calendar, originalItem);
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// onCancel
//////////////////////////////////////////////////////////////////////////////

function onCancel()
{
}

//////////////////////////////////////////////////////////////////////////////
// updateTitle
//////////////////////////////////////////////////////////////////////////////

function updateTitle()
{
  var isNew = window.calendarItem.isMutable;
  if (isEvent(window.calendarItem)) {
  if (isNew)
    document.title = calGetString("calendar", "newEventDialog");
  else
    document.title = calGetString("calendar", "editEventDialog");
  } else if (isToDo(window.calendarItem)) {
    if (isNew)
      document.title = calGetString("calendar", "newTaskDialog");
    else
      document.title = calGetString("calendar", "editTaskDialog");
  }
}

//////////////////////////////////////////////////////////////////////////////
// End of file
//////////////////////////////////////////////////////////////////////////////
