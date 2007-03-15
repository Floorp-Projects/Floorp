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
// Global variables
//////////////////////////////////////////////////////////////////////////////

// the following variables are constructed if the jsContext this file
// belongs to gets constructed. all those variables are meant to be accessed
// from within this file only.
var gStartTime = null;
var gEndTime = null;
var gItemDuration = null;
var gStartTimezone = null;
var gEndTimezone = null;
var gIsReadOnly = false;
var gUserID = null;
var gOrganizerID = null;
var gPrivacy = null;
var gURL = null;
var gPriority = 0;
var gDictCount = 0;
var gPrefs = null;
var gLastRepeatSelection = 0;
var gLastAlarmSelection = 0;
var gIgnoreUpdate = false;
var gShowTimeAs = null;
//var gConsoleService = null;

//////////////////////////////////////////////////////////////////////////////
// getString
//////////////////////////////////////////////////////////////////////////////

function getString(aBundleName, aStringName) {
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/"+aBundleName+".properties");
    return props.GetStringFromName(aStringName);
}

//////////////////////////////////////////////////////////////////////////////
// onLoad
//////////////////////////////////////////////////////////////////////////////

function onLoad()
{
  //gConsoleService = Components.classes["@mozilla.org/consoleservice;1"].getService(Components.interfaces.nsIConsoleService);

  // first of all retrieve the array of
  // arguments this window has been called with.
  var args = window.arguments[0];
  
  // the calling entity provides us with an object that is responsible
  // for recording details about the initiated modification. the 'finalize'-property
  // is our hook in order to receive a notification in case the operation needs
  // to be terminated prematurely. this function will be called if the calling
  // entity needs to immediately terminate the pending modification. in this
  // case we serialize the item and close the window.
  if(args.job) {

    // keep this context...
    var self = this;

    // store the 'finalize'-functor in the provided job-object.
    args.job.finalize = function() {

      // store any pending modifications...
      self.onAccept();

      var item = window.calendarItem;

      // ...and close the window.
      window.close();

      return item;
    }
  }

  window.fbWrapper = args.fbWrapper;

  // the most important attribute we expect from the
  // arguments is the item we'll edit in the dialog.
  var item = args.calendarEvent;

  // new items should have a non-empty title.
  if (item.isMutable) {
    if(!item.title || item.title.length <= 0) {
      if (isEvent(item)) {
        item.title = getString("sun-calendar-event-dialog","newEvent");
      } else {
        item.title = getString("sun-calendar-event-dialog","newTask");
      }
    }
  }

  window.onAcceptCallback = args.onOk;
  
  // we store the item in the window to be able
  // to access this from any location. please note
  // that the item is either an occurrence [proxy]
  // or the stand-alone item [single occurrence item].
  window.calendarItem = item;

  // we store the array of attendees in the window.
  // clone each existing attendee since we still suffer
  // from the 'lost x-properties'-bug.
  window.attendees = [];
  var attendees = item.getAttendees({});
  if(attendees && attendees.length) {
    for each (var attendee in attendees) {
      window.attendees.push(attendee.clone());
    }
  }
  
  // we store the organizer of the item in the window.
  // TODO: we clone the object since foreign X-props get lost
  // during the roundtrip. In order to detect whether or not
  // the item has been changed by the dialog we clone the organizer
  // in any case to get rid of the X-props.
  window.organizer = item.organizer ? item.organizer.clone() : null;

  window.isOccurrence = (item != item.parentItem);
  
  // we store the recurrence info in the window so it
  // can be accessed from any location. since the recurrence
  // info is a property of the parent item we need to check
  // whether or not this item is a proxy or a parent.
  var parentItem = item;
  if(parentItem.parentItem != parentItem) {
    parentItem = parentItem.parentItem;
  }
  window.recurrenceInfo = parentItem.recurrenceInfo;
  
  document.getElementById("sun-calendar-event-dialog").getButton("accept").setAttribute("collapsed","true");
  document.getElementById("sun-calendar-event-dialog").getButton("cancel").setAttribute("collapsed","true");
  document.getElementById("sun-calendar-event-dialog").getButton("cancel").parentNode.setAttribute("collapsed","true");

  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  gPrefs = prefService.getBranch(null);

  loadDialog(window.calendarItem);
  
  opener.setCursor("auto");

  document.getElementById("item-title").focus();
  document.getElementById("item-title").select();
}

//////////////////////////////////////////////////////////////////////////////
// dispose
//////////////////////////////////////////////////////////////////////////////

function dispose()
{
  var args = window.arguments[0];
  if(args.job && args.job.dispose)
    args.job.dispose();
}

//////////////////////////////////////////////////////////////////////////////
// onAccept
//////////////////////////////////////////////////////////////////////////////

function onAccept()
{
  dispose();

  onCommandSave();
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// onCommandCancel
//////////////////////////////////////////////////////////////////////////////

function onCommandCancel()
{
  // assume that new items need to be asked whether or
  // not the newly created item wants to be saved.
  var isNew = window.calendarItem.isMutable;
  if (!isNew) {
  
    var newItem = saveItem();
    var oldItem = window.calendarItem.clone();

    newItem.deleteProperty("DTSTAMP");
    oldItem.deleteProperty("DTSTAMP");

    // we need to guide the description text through the text-field since
    // newlines are getting converted which would indicate changes to the text.
    setElementValue("item-description", oldItem.getProperty("DESCRIPTION"));
    setItemProperty(oldItem, "DESCRIPTION", getElementValue("item-description"));
    
    var a = newItem.icalString;
    var b = oldItem.icalString;
    
    if(newItem.icalString == oldItem.icalString)
      return true;
  }
  
  // TODO: retrieve strings from dtd', etc.
  var promptService = 
           Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                     .getService(Components.interfaces.nsIPromptService);

  var promptTitle = "Save Event";
  var promptMessage = "Do you want to save changes?";
  var buttonLabel1 = "Yes";
  var buttonLabel2 = "No";
  
  var flags = promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0 +
              promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1 +
              promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_2;
  
  var choice = promptService.confirmEx(null, promptTitle, promptMessage, flags,
                                       buttonLabel1,null , buttonLabel2, null, {});

  switch(choice) {
      case 0:
        onCommandSave();
        return true;
      case 2:
        return true;
      default:
        return false;
  }
}

//////////////////////////////////////////////////////////////////////////////
// onCancel
//////////////////////////////////////////////////////////////////////////////

function onCancel()
{
  var result = onCommandCancel();
  if(result == true) {
    dispose();
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////
// timezoneString
//////////////////////////////////////////////////////////////////////////////

function timezoneString(date)
{
  var fragments = date.split('/');
  var num = fragments.length;
  if(num <= 1)
    return fragments[0];
  return fragments[num-2]+'/'+fragments[num-1];
}

//////////////////////////////////////////////////////////////////////////////
// loadDialog
//////////////////////////////////////////////////////////////////////////////

function loadDialog(item)
{
  setElementValue("item-title",item.title);
  setElementValue("item-location",item.getProperty("LOCATION"));

  loadDateTime(item);

  // add calendars to the calendar menulist
  var calendarList = document.getElementById("item-calendar");
  var calendars = getCalendarManager().getCalendars({});
  var calendarToUse = item.calendar || window.arguments[0].calendar
  var selectIndex = 0;
  for (i in calendars) {
      var calendar = calendars[i];
      if (calendar == item.calendar || calendar == window.arguments[0].calendar) {
        var menuitem = calendarList.appendItem(calendar.name, i);
        menuitem.calendar = calendar;
        if(calendarToUse) {
          if (calendarToUse.uri.equals(calendar.uri)) {
            calendarList.selectedIndex = selectIndex;
          }
        }
        selectIndex++;
      } else if (calendar && !calendar.readOnly) {
        var menuitem = calendarList.appendItem(calendar.name, i);
        menuitem.calendar = calendar;
        if(calendarToUse) {
          if (calendarToUse.uri.equals(calendar.uri)) {
            calendarList.selectedIndex = selectIndex;
          }
        }
        selectIndex++;
      }
  }
  
  // no calendar attached to item
  // select first entry in calendar list as default
  if (!calendarToUse) {
    document.getElementById("item-calendar").selectedIndex = 0;
  }

  // Categories
  var categoriesString = "Anniversary,Birthday,Business,Calls,Clients,Competition,Customer,Favorites,Follow up,Gifts,Holidays,Ideas,Issues,Miscellaneous,Personal,Projects,Public Holiday,Status,Suppliers,Travel,Vacation";
  try {
    var categories = getLocalizedPref("calendar.categories.names");
    if (categories && categories != "")
      categoriesString = categories;
  } catch (ex) {}
  var categoriesList = categoriesString.split( "," );

  // insert the category already in the menulist so it doesn't get lost
  var itemCategory = item.getProperty("CATEGORIES");
  if (itemCategory) {
      if (categoriesString.indexOf(itemCategory) == -1)
          categoriesList[categoriesList.length] = itemCategory;
  }
  categoriesList.sort();

  var oldMenulist = document.getElementById("item-categories");
  while (oldMenulist.hasChildNodes()) {
      oldMenulist.removeChild(oldMenulist.lastChild);
  }

  var categoryMenuList = document.getElementById("item-categories");
  var indexToSelect = 0;

  // Add a 'none' option to allow users to cancel the category
  var noneItem = categoryMenuList.appendItem(calGetString("calendar", "None"), "NONE");

  for (var i in categoriesList) {
      var catItem = categoryMenuList.appendItem(categoriesList[i], categoriesList[i]);
      catItem.value = categoriesList[i];
      if (itemCategory && categoriesList[i] == itemCategory) {
          indexToSelect = parseInt(i)+1;  // Add 1 because of 'None'
      }
  }

  categoryMenuList.selectedIndex = indexToSelect;

  // URL
  gURL = item.getProperty("URL");
  updateDocument();

  /* Status */
  setElementValue("item-description", item.getProperty("DESCRIPTION"));
  if (!isEvent(item)) {
      setElementValue("todo-status", item.getProperty("STATUS"));
  }

  /* Task completed date */
  if (item.completedDate) {
      updateToDoStatus(item.status, item.completedDate.jsDate);
  } else {
      updateToDoStatus(item.status);
  }

  /* Task percent complete */
  if (isToDo(item)) {
      var percentCompleteInteger = 0;
      var percentCompleteProperty = item.getProperty("PERCENT-COMPLETE");
      if (percentCompleteProperty != null) {
          percentCompleteInteger = parseInt(percentCompleteProperty);
      }
      if (percentCompleteInteger < 0) {
          percentCompleteInteger = 0;
      } else if (percentCompleteInteger > 100) {
          percentCompleteInteger = 100;
      }
      setElementValue("percent-complete-textbox", percentCompleteInteger);
  }

  // Priority
  gPriority = parseInt(item.priority);
  updatePriority();

  // Privacy
  gPrivacy = item.privacy;
  updatePrivacy();
    
  // load repeat details
  loadRepeat(item);
  
  // load reminder details
  loadReminder(item);

  // hide rows based on if this is an event or todo
  updateStyle();

  updateDateTime();

  updateCalendar();

  // figure out what the title of the dialog should be and set it
  updateTitle();

  updateAttendees();
  updateReminderDetails();
  
  gShowTimeAs = item.hasProperty("TRANSP") ?
    item.getProperty("TRANSP") : null;
  updateShowTimeAs();
}

//////////////////////////////////////////////////////////////////////////////
// loadDateTime
//////////////////////////////////////////////////////////////////////////////

function loadDateTime(item) {

  if (isEvent(item)) {

    var startTime = item.startDate;
    var endTime = item.endDate;
    var duration = endTime.subtractDate(startTime);
    
    // Check if an all-day event has been passed in (to adapt endDate).
    if (startTime.isDate) {

      startTime = startTime.clone();
      endTime = endTime.clone();

      endTime.day -= 1;
      duration.days -= 1;

      // The date/timepicker uses jsDate internally. Because jsDate does
      // not know the concept of dates we end up displaying times unequal
      // to 00:00 for all-day events depending on local timezone setting. 
      // Calling normalize() recalculates that times to represent 00:00
      // in local timezone.
      endTime.normalize();
      startTime.normalize();
    }

    // store the start/end-times as calIDateTime-objects
    // converted to the default timezone. store the timezones
    // separately.
    var kDefaultTimezone = calendarDefaultTimezone();
    gStartTimezone = startTime.timezone;
    gEndTimezone = endTime.timezone;
    gStartTime = startTime.getInTimezone(kDefaultTimezone);
    gEndTime = endTime.getInTimezone(kDefaultTimezone);
    gItemDuration = duration;
  }

  if (isToDo(item)) {

    var startTime = null;
    var endTime = null;
    var duration = null;
    
    var kDefaultTimezone = calendarDefaultTimezone();
    var hasEntryDate = (item.entryDate != null);
    if (hasEntryDate) {
      startTime = item.entryDate;
      gStartTimezone = startTime.timezone;
      startTime = startTime.getInTimezone(kDefaultTimezone);
    } else {
      gStartTimezone = kDefaultTimezone;
    }
    var hasDueDate = (item.dueDate != null);
    if (hasDueDate) {
      endTime = item.dueDate;
      gEndTimezone = endTime.timezone;
      endTime = endTime.getInTimezone(kDefaultTimezone);
    } else {
      gEndTimezone = kDefaultTimezone;
    }
    if (hasEntryDate && hasDueDate)
      duration = endTime.subtractDate(startTime);

    gStartTime = startTime;
    gEndTime = endTime;
    gItemDuration = duration;
  }
}

//////////////////////////////////////////////////////////////////////////////
// updateStartTime
//////////////////////////////////////////////////////////////////////////////

function updateStartTime()
{
  if(gIgnoreUpdate)
    return;

  var startWidgetId;
  var endWidgetId;
  if (isEvent(window.calendarItem)) {
    startWidgetId = "event-starttime";
    endWidgetId = "event-endtime";
  } else {
    if (!getElementValue("todo-has-entrydate", "checked"))
      gItemDuration = null;
    if (!getElementValue("todo-has-duedate", "checked"))
      gItemDuration = null;
    startWidgetId = "todo-entrydate";
    endWidgetId = "todo-duedate";
  }
  
  // jsDate is always in OS timezone, thus we create a calIDateTime
  // object from the jsDate representation and simply set the new
  // timezone instead of converting.
  var kDefaultTimezone = calendarDefaultTimezone();
  var start = jsDateToDateTime(getElementValue(startWidgetId));
  start = start.getInTimezone(kDefaultTimezone);
  var menuItem = document.getElementById('menu-options-timezone');
  if(menuItem.getAttribute('checked') == 'true') {
    start.timezone = gStartTimezone;
  }
  gStartTime = start.clone();
  if(gItemDuration) {
    start.addDuration(gItemDuration);
    start = start.getInTimezone(gEndTimezone);
  }
  if(gEndTime) {
    var menuItem = document.getElementById('menu-options-timezone');
    if(menuItem.getAttribute('checked') == 'true') {
      start.timezone = gEndTimezone
    }
    gEndTime = start;
  }

  var isAllDay = getElementValue("event-all-day", "checked");
  if(isAllDay)
    gStartTime.isDate = true;
  
  updateDateTime();
  updateTimezone();
}

//////////////////////////////////////////////////////////////////////////////
// updateEntryDate
//////////////////////////////////////////////////////////////////////////////

function updateEntryDate()
{
  if(gIgnoreUpdate)
    return;

  if (!isToDo(window.calendarItem))
      return;

  // force something to get set if there was nothing there before
  setElementValue("todo-entrydate", getElementValue("todo-entrydate"));

  // first of all disable the datetime picker if we don't have an entrydate
  var hasEntryDate = getElementValue("todo-has-entrydate", "checked");
  var hasDueDate = getElementValue("todo-has-duedate", "checked");
  setElementValue("todo-entrydate", !hasEntryDate, "disabled");

  // create a new datetime object if entrydate is now checked for the first time
  if (hasEntryDate && !gStartTime) {
    var kDefaultTimezone = calendarDefaultTimezone();
    var entryDate = jsDateToDateTime(getElementValue("todo-entrydate"));
    entryDate = entryDate.getInTimezone(kDefaultTimezone);
    gStartTime = entryDate;
  } else if (!hasEntryDate && gStartTime) {
    gStartTime = null;
  }

  // calculate the duration if possible
  if (hasEntryDate && hasDueDate) {
    var start = jsDateToDateTime(getElementValue("todo-entrydate"));
    var end = jsDateToDateTime(getElementValue("todo-duedate"));
    gItemDuration = end.subtractDate(start);
  } else {
    gItemDuration = null;
  }

  updateDateTime();
  updateTimezone();
}

//////////////////////////////////////////////////////////////////////////////
// updateEndTime
//////////////////////////////////////////////////////////////////////////////

function updateEndTime()
{
  if(gIgnoreUpdate)
    return;
    
  var startWidgetId;
  var endWidgetId;
  if (isEvent(window.calendarItem)) {
    startWidgetId = "event-starttime";
    endWidgetId = "event-endtime";
  } else {
    if (!getElementValue("todo-has-entrydate", "checked"))
      gItemDuration = null;
    if (!getElementValue("todo-has-duedate", "checked"))
      gItemDuration = null;
    startWidgetId = "todo-entrydate";
    endWidgetId = "todo-duedate";
  }

  var saveStartTime = gStartTime;
  var saveEndTime = gEndTime;
  var kDefaultTimezone = calendarDefaultTimezone();

  if(gStartTime) {
    var start = jsDateToDateTime(getElementValue(startWidgetId));
    start = start.getInTimezone(kDefaultTimezone);
    var menuItem = document.getElementById('menu-options-timezone');
    if(menuItem.getAttribute('checked') == 'true') {
      start.timezone = gStartTimezone;
    }
    gStartTime = start;
  }
  if(gEndTime) {
    var end = jsDateToDateTime(getElementValue(endWidgetId));
    end = end.getInTimezone(kDefaultTimezone);
    var timezone = gEndTimezone;
    if(timezone == "UTC") {
      if(gStartTime && gStartTimezone != gEndTimezone) {
        timezone = gStartTimezone;
      }
    }
    var menuItem = document.getElementById('menu-options-timezone');
    if(menuItem.getAttribute('checked') == 'true') {
      end.timezone = timezone;
    }
    gEndTime = end;
  }

  var isAllDay = getElementValue("event-all-day", "checked");
  if(isAllDay)
    gStartTime.isDate = true;
  
  // calculate the new duration of start/end-time.
  // don't allow for negative durations.
  var warning = false;
  if(gStartTime && gEndTime) {
    if(gEndTime.compare(gStartTime) >= 0) {
      gItemDuration = end.subtractDate(start);
    } else {
      gStartTime = saveStartTime;
      gEndTime = saveEndTime;
      warning = true;
    }
  }

  updateDateTime();
  updateTimezone();
  
  if(warning) {
    var callback = function func() {
      var promptService = 
               Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                         .getService(Components.interfaces.nsIPromptService);
      promptService.alert(null,document.title,"The end date you entered occurs before the start date.");
    }
    setTimeout(callback, 1);
  }
}

//////////////////////////////////////////////////////////////////////////////
// updateDueDate
//////////////////////////////////////////////////////////////////////////////

function updateDueDate()
{
  if(gIgnoreUpdate)
    return;

  if (!isToDo(window.calendarItem))
      return;

  // force something to get set if there was nothing there before
  setElementValue("todo-duedate", getElementValue("todo-duedate"));

  // first of all disable the datetime picker if we don't have a duedate
  var hasEntryDate = getElementValue("todo-has-entrydate", "checked");
  var hasDueDate = getElementValue("todo-has-duedate", "checked");
  setElementValue("todo-duedate",!hasDueDate,"disabled");

  // create a new datetime object if duedate is now checked for the first time
  if (hasDueDate && !gEndTime) {
    var kDefaultTimezone = calendarDefaultTimezone();
    var dueDate = jsDateToDateTime(getElementValue("todo-duedate"));
    dueDate = dueDate.getInTimezone(kDefaultTimezone);
    gEndTime = dueDate;
  } else if (!hasDueDate && gEndTime) {
    gEndTime = null;
  }
  
  // calculate the duration if possible
  if (hasEntryDate && hasDueDate) {
    var start = jsDateToDateTime(getElementValue("todo-entrydate"));
    var end = jsDateToDateTime(getElementValue("todo-duedate"));
    gItemDuration = end.subtractDate(start);
  } else {
    gItemDuration = null;
  }

  updateDateTime();
  updateTimezone();
}

//////////////////////////////////////////////////////////////////////////////
// loadRepeat
//////////////////////////////////////////////////////////////////////////////

function loadRepeat(item)
{
  var recurrenceInfo = window.recurrenceInfo;
  setElementValue("item-repeat", "none");
  if (recurrenceInfo) {
    setElementValue("item-repeat", "custom");
    var ritems = recurrenceInfo.getRecurrenceItems({});
    var rules = [];
    var exceptions = [];
    for each (var r in ritems) {
        if (r.isNegative)
            exceptions.push(r);
        else
            rules.push(r);
    }
    if (rules.length == 1) {
      var rule = rules[0];
      if (rule instanceof Ci.calIRecurrenceRule) {
        switch(rule.type) {
          case 'DAILY':
            if (rule.interval == 1) {
              if (!rule.isFinite) {
                if (!checkRecurrenceRule(rule,['BYSECOND',
                                                    'BYMINUTE',
                                                    'BYHOUR',
                                                    'BYMONTHDAY',
                                                    'BYYEARDAY',
                                                    'BYWEEKNO',
                                                    'BYMONTH',
                                                    'BYSETPOS'])) {
                  var ruleComp = rule.getComponent("BYDAY",{});
                  if (ruleComp.length > 0) {
                    if (ruleComp.length == 5) {
                      for (var i=0; i<5; i++) {
                        if (ruleComp[i] != i+2)
                          break;
                      }
                      if (i==5)
                        setElementValue("item-repeat", "every.weekday");
                    }
                  }
                  else {
                    setElementValue("item-repeat", "daily");
                  }
                }
              }
            }
            break;
            
          case 'WEEKLY':
            if (!checkRecurrenceRule(rule,['BYSECOND',
                                                'BYMINUTE',
                                                'BYDAY',
                                                'BYHOUR',
                                                'BYMONTHDAY',
                                                'BYYEARDAY',
                                                'BYWEEKNO',
                                                'BYMONTH',
                                                'BYSETPOS'])) {
              if (rule.interval == 1) {
                if (!rule.isFinite) {
                  setElementValue("item-repeat", "weekly");
                }
              } else if (rule.interval == 2) {
                if (!rule.isFinite) {
                  setElementValue("item-repeat", "bi.weekly");
                }
              }
            }
            break;

          case 'MONTHLY':
            if (!checkRecurrenceRule(rule,['BYSECOND',
                                                'BYMINUTE',
                                                'BYDAY',
                                                'BYHOUR',
                                                'BYMONTHDAY',
                                                'BYYEARDAY',
                                                'BYWEEKNO',
                                                'BYMONTH',
                                                'BYSETPOS'])) {
              if (rule.interval == 1) {
                if (!rule.isFinite) {
                  setElementValue("item-repeat", "monthly");
                }
              }
            }
            break;
            
          case 'YEARLY':
            if (!checkRecurrenceRule(rule,['BYSECOND',
                                                'BYMINUTE',
                                                'BYDAY',
                                                'BYHOUR',
                                                'BYMONTHDAY',
                                                'BYYEARDAY',
                                                'BYWEEKNO',
                                                'BYMONTH',
                                                'BYSETPOS'])) {
              if (rule.interval == 1) {
                if (!rule.isFinite) {
                  setElementValue("item-repeat", "yearly");
                }
              }
            }
            break;
        }
      }
    }
  }

  var repeatMenu = document.getElementById("item-repeat");
  gLastRepeatSelection = repeatMenu.selectedIndex;

  if(item.parentItem != item) {
    disableElement("item-repeat");
  }
}

//////////////////////////////////////////////////////////////////////////////
// checkRecurrenceRule
//////////////////////////////////////////////////////////////////////////////

function checkRecurrenceRule(rule,array)
{
  for each (var comp in array) {
    var ruleComp = rule.getComponent(comp,{});
    if(ruleComp && ruleComp.length > 0)
      return true;
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////////
// loadReminder
//////////////////////////////////////////////////////////////////////////////

function loadReminder(item)
{
  // select 'no reminder' by default
  var reminderPopup = document.getElementById("item-alarm");
  reminderPopup.selectedIndex = 0;
  gLastAlarmSelection = 0;
  if (!item.alarmOffset)
    return;
  
  // try to match the reminder setting with the available popup items
  var matchingItem = null;  
  var menuItems = reminderPopup.getElementsByTagName("menuitem");
  var numItems = menuItems.length;
  for(var i=0; i<numItems; i++) {
    var menuitem = menuItems[i];
    if(menuitem.hasAttribute("length")) {
      var origin = "1";
      if(item.alarmRelated == Components.interfaces.calIItemBase.ALARM_RELATED_END)
        origin = "-1";
      var duration = item.alarmOffset.clone();
      var relation = "END";
      if(duration.isNegative) {
        duration.isNegative = false;
        duration.normalize();
        relation = "START";
      }
      if(menuitem.getAttribute("origin") == origin) {
        if(menuitem.getAttribute("relation") == relation) {
          var unit = menuitem.getAttribute("unit");
          var length = menuitem.getAttribute("length");
          if(unit == "minutes" && item.alarmOffset.minutes == length) {
            matchingItem = menuitem;
            break;
          } else if(unit == "hours" && item.alarmOffset.hours == length) {
            matchingItem = menuitem;
            break;
          } else if(unit == "days" && item.alarmOffset.days == length) {
            matchingItem = menuitem;
            break;
          }
        }
      }
    }
  }

  if(matchingItem) {
  
    var numChilds = reminderPopup.childNodes[0].childNodes.length;
    for(var i=0; i<numChilds; i++) {
      var node = reminderPopup.childNodes[0].childNodes[i];
      if(node == matchingItem) {
        reminderPopup.selectedIndex = i;
        break;
      }
    }
  }
  else {
  
    reminderPopup.value = 'custom';
    var customReminder = document.getElementById("reminder-custom-menuitem");
    var reminder = {};
    if(item.alarmRelated == Components.interfaces.calIItemBase.ALARM_RELATED_START) {
      reminder.origin = "1";
    } else {
      reminder.origin = "-1";
    }
    var offset = item.alarmOffset.clone();
    var relation = "END";
    if(offset.isNegative) {
      offset.isNegative = false;
      offset.normalize();
      relation = "START";
    }
    reminder.relation = relation;
    if (offset.minutes) {
      var minutes = offset.minutes + offset.hours*60 + offset.days*24*60 + offset.weeks*60*24*7;
      reminder.unit = 'minutes';
      reminder.length = minutes;
    } else if (offset.hours) {
      var hours = offset.hours + offset.days*24 + offset.weeks*24*7;
      reminder.unit = 'hours';
      reminder.length = hours;
    } else {
      var days = offset.days + offset.weeks*7;
      reminder.unit = 'days';
      reminder.length = days;
    }
    customReminder.reminder = reminder;
  }

  // remember the selected index
  gLastAlarmSelection = reminderPopup.selectedIndex;
}

//////////////////////////////////////////////////////////////////////////////
// saveReminder
//////////////////////////////////////////////////////////////////////////////

function saveReminder(item) {
  
  var reminderPopup = document.getElementById("item-alarm");
  if (reminderPopup.value == 'none') {

    item.alarmOffset = null;
    item.alarmLastAck = null;
    item.alarmRelated = null;
  }
  else {
  
    var menuitem = reminderPopup.selectedItem;
    
    // custom reminder entries carry their own reminder object
    // with them, pre-defined entries specify the necessary information
    // as attributes attached to the menuitem elements.
    var reminder = menuitem.reminder;
    if (!reminder) {
      reminder = {};
      reminder.length = menuitem.getAttribute('length');
      reminder.unit = menuitem.getAttribute('unit');
      reminder.relation = menuitem.getAttribute('relation');
      reminder.origin = menuitem.getAttribute('origin');
    }

    var duration = Components.classes["@mozilla.org/calendar/duration;1"]
                             .createInstance(Components.interfaces.calIDuration);
    duration[reminder.unit] = Number(reminder.length);
    if (reminder.relation != "END") {
      duration.isNegative = true;
    }
    duration.normalize();
    item.alarmOffset = duration;

    if (Number(reminder.origin) >= 0) {
        item.alarmRelated = Components.interfaces.calIItemBase.ALARM_RELATED_START;
    } else {
        item.alarmRelated = Components.interfaces.calIItemBase.ALARM_RELATED_END;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// editReminder
//////////////////////////////////////////////////////////////////////////////

function editReminder()
{
  var customReminder = document.getElementById("reminder-custom-menuitem");
  var args = new Object();
  args.reminder = customReminder.reminder;
  var savedWindow = window;
  args.onOk = function(reminder) {
    customReminder.reminder = reminder;
  };

  window.setCursor("wait");

  // open the dialog modally
  openDialog("chrome://calendar/content/sun-calendar-event-dialog-reminder.xul", "_blank", "chrome,titlebar,modal,resizable", args);
}

//////////////////////////////////////////////////////////////////////////////
// updateReminder
//////////////////////////////////////////////////////////////////////////////

function updateReminder()
{
  // TODO: possibly the selected reminder conflicts with the item.
  // for example an end-relation combined with a task without duedate.
  // we need to disable the ok-button in this case.

  // find relevant elements in the document
  var reminderPopup = document.getElementById("item-alarm");
  var reminderDetails = document.getElementById("reminder-details");

  // if a custom reminder was selected, we show the appropriate
  // dialog in order to allow the user to specify the details.
  // the result will be placed in the 'reminder-custom-menuitem' tag.
  if(reminderPopup.value == 'custom') {
  
    // show the dialog.
    editReminder();
    
    // now check if the resulting custom reminder is valid.
    // possibly we receive an invalid reminder if the user cancels the dialog.
    // in that case we revert to the previous selection of the reminder drop down.
    var reminder = document.getElementById("reminder-custom-menuitem").reminder;
    if (!reminder) {
      reminderPopup.selectedIndex = gLastAlarmSelection;
    }
  }

  // remember the current reminder drop down selection index.
  gLastAlarmSelection = reminderPopup.selectedIndex;

  updateReminderDetails();
  updateAccept();
}

//////////////////////////////////////////////////////////////////////////////
// updateReminderDetails
//////////////////////////////////////////////////////////////////////////////

function updateReminderDetails()
{
  // find relevant elements in the document
  var reminderPopup = document.getElementById("item-alarm");
  var reminderDetails = document.getElementById("reminder-details");
  
  // first of all collapse the details text. if we fail to
  // create a details string, we simply don't show anything.
  reminderDetails.setAttribute("collapsed","true");
  
  // don't try to show the details text
  // for anything but a custom recurrence rule.
  if(reminderPopup.value == "custom") {

    var reminder = document.getElementById("reminder-custom-menuitem").reminder;
    
    if(reminder) {

      var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                          .getService(Components.interfaces.nsIStringBundleService);
      var props = sbs.createBundle("chrome://calendar/locale/sun-calendar-event-dialog.properties");

      var unitString;
      switch(reminder.unit) {
        case 'minutes':
          unitString = Number(reminder.length) <= 1 ?
            props.GetStringFromName('reminderCustomUnitMinute') :
            props.GetStringFromName('reminderCustomUnitMinutes');
          break;
        case 'hours':
          unitString = Number(reminder.length) <= 1 ?
            props.GetStringFromName('reminderCustomUnitHour') :
            props.GetStringFromName('reminderCustomUnitHours');
          break;
        case 'days':
          unitString = Number(reminder.length) <= 1 ?
            props.GetStringFromName('reminderCustomUnitDay') :
            props.GetStringFromName('reminderCustomUnitDays');
          break;
      }

      var relationString;
      switch(reminder.relation) {
        case 'START':
          relationString = props.GetStringFromName('reminderCustomRelationStart');
          break;
        case 'END':
          relationString = props.GetStringFromName('reminderCustomRelationEnd');
          break;
      }

      var originString;
      if(reminder.origin && reminder.origin < 0) {
        originString = props.GetStringFromName('reminderCustomOriginEnd');
      } else {
        originString = props.GetStringFromName('reminderCustomOriginBegin');
      }

      var detailsString = props.formatStringFromName(
        'reminderCustomTitle',
        [ reminder.length,
          unitString,
          relationString,
          originString], 4);

      var lines = detailsString.split("\n");
      reminderDetails.removeAttribute("collapsed");
      while(reminderDetails.childNodes.length > lines.length)
        reminderDetails.removeChild(reminderDetails.lastChild);
      var numChilds = reminderDetails.childNodes.length;
      for (var i = 0; i < lines.length; i++) {
        if(i >= numChilds) {
          var newNode = reminderDetails.childNodes[0].cloneNode(true);
          reminderDetails.appendChild(newNode);
        }
        var node = reminderDetails.childNodes[i];
        node.setAttribute('value',lines[i]);
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// saveDialog
//////////////////////////////////////////////////////////////////////////////

function saveDialog(item)
{
  setItemProperty(item,"title", getElementValue("item-title"));
  setItemProperty(item,"LOCATION",getElementValue("item-location"));

  saveDateTime(item);

  if (isToDo(item)) {

    var percentCompleteInteger = 0;
    if (getElementValue("percent-complete-textbox") != "") {
        percentCompleteInteger = parseInt(getElementValue("percent-complete-textbox"));
    }
    if (percentCompleteInteger < 0) {
        percentCompleteInteger = 0;
    } else if (percentCompleteInteger > 100) {
        percentCompleteInteger = 100;
    }
    setItemProperty(item, "PERCENT-COMPLETE", percentCompleteInteger);
  }

  // Category
  var category = getElementValue("item-categories");

  if (category != "NONE") {
     setItemProperty(item, "CATEGORIES", category);
  } else {
     item.deleteProperty("CATEGORIES");
  }

  // URL
  setItemProperty(item,"URL",gURL);
  
  setItemProperty(item, "DESCRIPTION", getElementValue("item-description"));

  if (!isEvent(item)) {
      var status = getElementValue("todo-status");
      if (status != "COMPLETED") {
          item.completedDate = null;
      }
      setItemProperty(item, "STATUS",   status);
  }

  // set the "PRIORITY" property if a valid priority has been
  // specified (any integer value except *null*) OR the item
  // already specifies a priority. in any other case we don't
  // need this property and can safely delete it. we need this special
  // handling since the WCAP provider always includes the priority
  // with value *null* and we don't detect changes to this item if
  // we delete this property.
  if (gPriority || item.hasProperty("PRIORITY")) {
    item.setProperty("PRIORITY",gPriority);
  } else {
    item.deleteProperty("PRIORITY");
  }

  if(gShowTimeAs) {
    item.setProperty("TRANSP",gShowTimeAs);
  } else {
    item.deleteProperty("TRANSP");
  }

  setItemProperty(item,"CLASS",gPrivacy);

  if (item.status == "COMPLETED" && isToDo(item)) {
      item.completedDate = jsDateToDateTime(getElementValue("completed-date-picker"));
  }

  saveReminder(item);
}

//////////////////////////////////////////////////////////////////////////////
// saveDateTime
//////////////////////////////////////////////////////////////////////////////

function saveDateTime(item) {

  var kDefaultTimezone = calendarDefaultTimezone();

  if (isEvent(item)) {

    var startTime = gStartTime.getInTimezone(gStartTimezone);
    var endTime = gEndTime.getInTimezone(gEndTimezone);

    var isAllDay = getElementValue("event-all-day", "checked");
    if (isAllDay) {

      startTime = startTime.clone();
      endTime = endTime.clone();

      startTime.isDate = true;
      startTime.normalize();

      endTime.isDate = true;
      endTime.day += 1;
      endTime.normalize();
    
    } else {

      startTime = startTime.clone();
      startTime.isDate = false;
      endTime = endTime.clone();
      endTime.isDate = false;
    }

    setItemProperty(item,"startDate",startTime);
    setItemProperty(item,"endDate",endTime);
  }

  if (isToDo(item)) {

    var startTime = gStartTime ? gStartTime.getInTimezone(gStartTimezone) : null;
    var endTime = gEndTime ? gEndTime.getInTimezone(gEndTimezone) : null;

    setItemProperty(item,"entryDate",startTime);
    setItemProperty(item,"dueDate",endTime);
  }
}

//////////////////////////////////////////////////////////////////////////////
// updateTitle
//////////////////////////////////////////////////////////////////////////////

function updateTitle()
{
  var title = "";
  var isNew = window.calendarItem.isMutable;
  if (isEvent(window.calendarItem)) {
    if (isNew) {
      title = calGetString("calendar", "newEventDialog");
    } else {
      title = calGetString("calendar", "editEventDialog");
    }
  } else if (isToDo(window.calendarItem)) {
    if (isNew) {
      title = calGetString("calendar", "newTaskDialog");
    } else {
      title = calGetString("calendar", "editTaskDialog");
    }
  }
  title += ': ';
  title += getElementValue("item-title");
  document.title = title;
}

//////////////////////////////////////////////////////////////////////////////
// updateStyle
//////////////////////////////////////////////////////////////////////////////

function updateStyle()
{
  const kDialogStylesheet = "chrome://calendar/content/sun-calendar-event-dialog.css";

  for each(var stylesheet in document.styleSheets) {
      if (stylesheet.href == kDialogStylesheet) {
          if (isEvent(window.calendarItem))
              stylesheet.insertRule(".todo-only { display: none; }", 0);
          else if (isToDo(window.calendarItem))
              stylesheet.insertRule(".event-only { display: none; }", 0);
          return;
      }
  }
}

//////////////////////////////////////////////////////////////////////////////
// updateAccept
//////////////////////////////////////////////////////////////////////////////

function updateAccept()
{
  var enableAccept = true;

  var kDefaultTimezone = calendarDefaultTimezone();

  // don't allow for end dates to be before start dates
  var startDate;
  var endDate;
  if (isEvent(window.calendarItem)) {

      startDate = jsDateToDateTime(getElementValue("event-starttime"));
      endDate = jsDateToDateTime(getElementValue("event-endtime"));

      var menuItem = document.getElementById('menu-options-timezone');
      if(menuItem.getAttribute('checked') == 'true') {

        startTimezone = gStartTimezone;
        endTimezone = gEndTimezone;
        if(endTimezone == "UTC") {
          if(gStartTimezone != gEndTimezone) {
            endTimezone = gStartTimezone;
          }
        }

        startDate = startDate.getInTimezone(kDefaultTimezone);
        endDate = endDate.getInTimezone(kDefaultTimezone);

        startDate.timezone = startTimezone;
        endDate.timezone = endTimezone;
      }

      startDate = startDate.getInTimezone(kDefaultTimezone);
      endDate = endDate.getInTimezone(kDefaultTimezone);

      // For all-day events we are not interested in times and compare only dates.
      if (getElementValue("event-all-day", "checked")) {
          // jsDateToDateTime returnes the values in UTC. Depending on the local
          // timezone and the values selected in datetimepicker the date in UTC
          // might be shifted to the previous or next day.
          // For example: The user (with local timezone GMT+05) selected 
          // Feb 10 2006 00:00:00. The corresponding value in UTC is
          // Feb 09 2006 19:00:00. If we now set isDate to true we end up with
          // a date of Feb 09 2006 instead of Feb 10 2006 resulting in errors
          // during the following comparison.
          // Calling getInTimezone() ensures that we use the same dates as 
          // displayed to the user in datetimepicker for comparison.
          startDate.isDate = true;
          endDate.isDate = true;
      }
  } else {
      startDate = getElementValue("todo-has-entrydate", "checked") ? 
          jsDateToDateTime(getElementValue("todo-entrydate")) : null;
      endDate = getElementValue("todo-has-duedate", "checked") ? 
          jsDateToDateTime(getElementValue("todo-duedate")) : null;
  }

  if (endDate && startDate && endDate.compare(startDate) == -1) {
      enableAccept = false;
  }

  // can't add/edit items in readOnly calendars
  //if (gIsReadOnly)
    //showWarning("read-only-item");
  //var cal = document.getElementById("item-calendar").selectedItem.calendar;
  //if (cal.readOnly)
    //showWarning("read-only-cal");
  //if (gIsReadOnly || cal.readOnly) {
      //enableAccept = false;
  //}

  if (!updateTaskAlarmWarnings())
      enableAccept = false;

  var accept = document.getElementById("cmd_accept");
  if(enableAccept) {
    accept.removeAttribute('disabled');
  } else {
    accept.setAttribute('disabled','true');
  }

  return enableAccept;
}

//////////////////////////////////////////////////////////////////////////////
// updateTaskAlarmWarnings
//////////////////////////////////////////////////////////////////////////////

function updateTaskAlarmWarnings()
{
  var alarmType = getElementValue("item-alarm");
  if (!isToDo(window.calendarItem) || alarmType == "none") {
      return true;
  }

  var hasEntryDate = getElementValue("todo-has-entrydate", "checked");
  var hasDueDate = getElementValue("todo-has-duedate", "checked");

  var alarmRelated = document.getElementById("alarm-trigger-relation").selectedItem.value;

  if ((alarmType != "custom" || alarmRelated == "START") && !hasEntryDate) {
    return false;
  }

  if (alarmRelated == "END" && !hasDueDate) {
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////
// updateAllDay
//////////////////////////////////////////////////////////////////////////////

// this function sets the enabled/disabled
// state of the following controls:
// - 'event-starttime'
// - 'event-endtime'
// - 'timezone-starttime'
// - 'timezone-endtime'
// the state depends on whether or not the
// event is configured as 'all-day' or not.
function updateAllDay()
{
  if(gIgnoreUpdate)
    return;

  //gConsoleService.logStringMessage("### updateAllDay()");

  if (!isEvent(window.calendarItem))
      return;

  var allDay = getElementValue("event-all-day", "checked");
  setElementValue("event-starttime", allDay, "timepickerdisabled");
  setElementValue("event-endtime", allDay, "timepickerdisabled");

  var tzStart = document.getElementById("timezone-starttime");
  var tzEnd = document.getElementById("timezone-endtime");

  // automatically select "show time as free" if this
  // event is said to be all-day.
  if(allDay) {
    gShowTimeAs = "TRANSPARENT";
    updateShowTimeAs();
  }

  gStartTime.isDate = allDay;
  gEndTime.isDate = allDay;

  // disable the timezone links if 'allday' is checked OR the
  // calendar of this item is read-only. in any other case we
  // enable the links.
  if(allDay || gIsReadOnly) {
    tzStart.setAttribute("disabled","true");
    tzEnd.setAttribute("disabled","true");
    tzStart.removeAttribute("class");
    tzEnd.removeAttribute("class");
  } else {
    tzStart.removeAttribute("disabled");
    tzEnd.removeAttribute("disabled");
    tzStart.setAttribute("class","text-link");
    tzEnd.setAttribute("class","text-link");
  }

  updateDateTime();
  updateRepeatDetails();
  updateAccept();
}

//////////////////////////////////////////////////////////////////////////////
// setAlarmFields
//////////////////////////////////////////////////////////////////////////////

function setAlarmFields(alarmItem)
{
  var alarmLength = alarmItem.getAttribute("length");
  if (alarmLength != "") {
      var alarmUnits = alarmItem.getAttribute("unit");
      var alarmRelation = alarmItem.getAttribute("relation");
      setElementValue("alarm-length-field", alarmLength);
      setElementValue("alarm-length-units", alarmUnits);
      setElementValue("alarm-trigger-relation", alarmRelation);
  }
}

//////////////////////////////////////////////////////////////////////////////
// openNewEvent
//////////////////////////////////////////////////////////////////////////////

function openNewEvent()
{
  var item = window.calendarItem;
  var args = window.arguments[0];
  args.onNewEvent(item.calendar);
}

//////////////////////////////////////////////////////////////////////////////
// openNewMessage
//////////////////////////////////////////////////////////////////////////////

function openNewMessage()
{
  var msgComposeService = Components.classes["@mozilla.org/messengercompose;1"].getService();
  msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);
  msgComposeService.OpenComposeWindow(null, null,
                                      Components.interfaces.nsIMsgCompType.New,
                                      Components.interfaces.nsIMsgCompFormat.Default,
                                      null, null);
}

//////////////////////////////////////////////////////////////////////////////
// openNewCardDialog
//////////////////////////////////////////////////////////////////////////////

function openNewCardDialog()
{
  window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul","","chrome,resizable=no,titlebar,modal");
}


//////////////////////////////////////////////////////////////////////////////
// editAttendees
//////////////////////////////////////////////////////////////////////////////

function editAttendees()
{
  var savedWindow = window;
  var calendar = document.getElementById("item-calendar").selectedItem.calendar;
  
  var callback = function(attendees,organizer,startTime,endTime) {
    savedWindow.attendees = attendees;
    savedWindow.organizer = organizer;
    var duration = endTime.subtractDate(startTime);
    startTime = startTime.clone();
    endTime = endTime.clone();
    endTime.normalize();
    startTime.normalize();
    var kDefaultTimezone = calendarDefaultTimezone();
    gStartTimezone = startTime.timezone;
    gEndTimezone = endTime.timezone;
    gStartTime = startTime.getInTimezone(kDefaultTimezone);
    gEndTime = endTime.getInTimezone(kDefaultTimezone);
    gItemDuration = duration;
    updateAttendees();
    updateDateTime();
  };

  var startTime = gStartTime.getInTimezone(gStartTimezone);
  var endTime = gEndTime.getInTimezone(gEndTimezone);

  var isAllDay = getElementValue("event-all-day", "checked");
  if (isAllDay) {
    startTime.isDate = true;
    startTime.normalize();
    endTime.isDate = true;
    endTime.day += 1;
    endTime.normalize();
  } else {
    startTime.isDate = false;
    endTime.isDate = false;
  }

  var menuItem = document.getElementById('menu-options-timezone');
  var displayTimezone = menuItem.getAttribute('checked') == 'true';

  var args = new Object();
  args.startTime = startTime;
  args.endTime = endTime;
  args.displayTimezone = displayTimezone;
  args.attendees = window.attendees;
  args.organizer = window.organizer;
  args.calendar = calendar;
  args.item = window.calendarItem;
  args.onOk = callback;
  args.fbWrapper = window.fbWrapper;

  // open the dialog modally
  openDialog("chrome://calendar/content/sun-calendar-event-dialog-attendees.xul", "_blank", "chrome,titlebar,modal,resizable", args);
}

//////////////////////////////////////////////////////////////////////////////
// editPrivacy
//////////////////////////////////////////////////////////////////////////////

function editPrivacy(target)
{
  gPrivacy = target.getAttribute("value");

  if(gPrivacy == "PRIVATE")
    gShowTimeAs = "TRANSPARENT";
  else if(gPrivacy == "CONFIDENTIAL")
    gShowTimeAs = "OPAQUE";
  else if(gPrivacy == "PUBLIC")
    gShowTimeAs = "OPAQUE";
     
  updateShowTimeAs();
  updatePrivacy();  
}

//////////////////////////////////////////////////////////////////////////////
// updatePrivacy
//////////////////////////////////////////////////////////////////////////////

// this function updates the UI according to the global field 'gPrivacy'.
// in case 'gPrivacy' is modified updatePrivacy() should be called to
// reflect the modification in the UI.
function updatePrivacy()
{
  var privacyPublic = document.getElementById("cmd_privacy_public");
  var privacyConfidential = document.getElementById("cmd_privacy_confidential");
  var privacyPrivate = document.getElementById("cmd_privacy_private");
  
  privacyPublic.setAttribute("checked",privacyPublic.getAttribute("value") == gPrivacy ? "true" : "false");
  privacyConfidential.setAttribute("checked",privacyConfidential.getAttribute("value") == gPrivacy ? "true" : "false");
  privacyPrivate.setAttribute("checked",privacyPrivate.getAttribute("value") == gPrivacy ? "true" : "false");

  var statusbar = document.getElementById("status-bar");
  var numChilds = statusbar.childNodes.length;
  for(var i=0; i<numChilds; i++) {
    var node = statusbar.childNodes[i];
    if(node.hasAttribute("privacy")) {
      if(gPrivacy != node.getAttribute("privacy")) {
        node.setAttribute("collapsed","true");
      } else {
        node.removeAttribute("collapsed");
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// editPriority
//////////////////////////////////////////////////////////////////////////////

function editPriority(target)
{
  gPriority = parseInt(target.getAttribute("value"));

  updatePriority();  
}

//////////////////////////////////////////////////////////////////////////////
// updatePriority
//////////////////////////////////////////////////////////////////////////////

function updatePriority()
{
  var priorityLevel = "none";
  if (gPriority >= 1 && gPriority <= 4) {
      priorityLevel = "high";
  } else if (gPriority == 5) {
      priorityLevel = "normal";
  } else if (gPriority >= 6 && gPriority <= 9) {
      priorityLevel = "low";
  }

  var priorityNone = document.getElementById("cmd_priority_none");
  var priorityLow = document.getElementById("cmd_priority_low");
  var priorityNormal = document.getElementById("cmd_priority_normal");
  var priorityHigh = document.getElementById("cmd_priority_high");
  
  priorityNone.setAttribute("checked",priorityLevel == "none" ? "true" : "false");
  priorityLow.setAttribute("checked",priorityLevel == "low" ? "true" : "false");
  priorityNormal.setAttribute("checked",priorityLevel == "normal" ? "true" : "false");
  priorityHigh.setAttribute("checked",priorityLevel == "high" ? "true" : "false");

  var priority = document.getElementById("status-priority");
  var collapse = priorityLevel == "none" ? true : false;
  var numChilds = priority.childNodes.length;
  for(var i=0; i<numChilds; i++) {
    var node = priority.childNodes[i];
    if(collapse) {
      node.setAttribute("collapsed","true");
    } else {
      node.removeAttribute("collapsed");
    }
    if(node.getAttribute("value") == priorityLevel) {
      collapse = true;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// editShowTimeAs
//////////////////////////////////////////////////////////////////////////////

function editShowTimeAs(target)
{
  gShowTimeAs = target.getAttribute("value");
  
  updateShowTimeAs();  
}

//////////////////////////////////////////////////////////////////////////////
// updateShowTimeAs
//////////////////////////////////////////////////////////////////////////////

function updateShowTimeAs()
{
  var showAsBusy = document.getElementById("cmd_showtimeas_busy");
  var showAsFree = document.getElementById("cmd_showtimeas_free");
  
  showAsBusy.setAttribute("checked",gShowTimeAs == "OPAQUE" ? "true" : "false");
  showAsFree.setAttribute("checked",gShowTimeAs == "TRANSPARENT" ? "true" : "false");
}

//////////////////////////////////////////////////////////////////////////////
// editURL
//////////////////////////////////////////////////////////////////////////////

function editURL()
{
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                .getService(Components.interfaces.nsIPromptService);
  if (promptService) {
    var result = {value:gURL};
    if (promptService.prompt( window,"Please specify the document location","Target:",result,null,{value:0})) {
      var url = result.value;
      // The user might have just put in 'www.foo.com', correct that here
      if (url != "" && url.indexOf( ":" ) == -1) {
         url = "http://" + url;
      }
      gURL = url;
      updateDocument();
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// setItemProperty
//////////////////////////////////////////////////////////////////////////////

function setItemProperty(item,propertyName,value)
{
  switch(propertyName) {
    case "startDate":
      if (value.isDate && !item.startDate.isDate ||
          !value.isDate && item.startDate.isDate ||
          value.timezone != item.startDate.timezone ||
          value.compare(item.startDate) != 0)
          item.startDate = value;
      break;
    case "endDate":
      if (value.isDate && !item.endDate.isDate ||
          !value.isDate && item.endDate.isDate ||
          value.timezone != item.endDate.timezone ||
          value.compare(item.endDate) != 0)
          item.endDate = value;
      break;

    case "entryDate":
      if (value == item.entryDate)
          break;
      if ((value && !item.entryDate) ||
          (!value && item.entryDate) ||
          (value.timezone != item.entryDate.timezone) ||
          (value.compare(item.entryDate) != 0))
          item.entryDate = value;
      break;
    case "dueDate":
      if (value == item.dueDate)
          break;
      if ((value && !item.dueDate) ||
          (!value && item.dueDate) ||
          (value.timezone != item.dueDate.timezone) ||
          (value.compare(item.dueDate) != 0))
          item.dueDate = value;
      break;
    case "isCompleted":
      if (value != item.isCompleted)
          item.isCompleted = value;
      break;

    case "title":
      if (value != item.title)
          item.title = value;
      break;

    default:
      if (!value || value == "")
          item.deleteProperty(propertyName);
      else if (item.getProperty(propertyName) != value)
          item.setProperty(propertyName, value);
      break;
  }
}


//////////////////////////////////////////////////////////////////////////////
// updateCalendar
//////////////////////////////////////////////////////////////////////////////

function updateCalendar()
{
  var item = window.calendarItem;
  var calendar = document.getElementById("item-calendar").selectedItem.calendar;

  gIsReadOnly = true;
  if(calendar)
      gIsReadOnly = calendar.readOnly;

  // update the accept button
  updateAccept();

  // TODO: the code above decided about whether or not the item is readonly.
  // below we enable/disable all controls based on this decision. unfortunately
  // some controls need to be disabled based on some other criteria. this is why
  // we enable all controls in case the item is *not* readonly and run through
  // all those updateXXX() functions to disable them again based on the
  // specific logic build into those function. is this really a good idea?
  if(gIsReadOnly) {
    var disableElements = document.getElementsByAttribute("disable-on-readonly", "true");
    for (var i=0;i<disableElements.length;i++) {
      disableElements[i].setAttribute('disabled', 'true');
      
      // we mark link-labels with the hyperlink attribute, since we need to
      // remove their class in case they get disabled. TODO: it would be better
      // to create a small binding for those link-labels instead of adding
      // those special stuff.
      if(disableElements[i].hasAttribute('hyperlink')) {
        disableElements[i].removeAttribute('class');
        disableElements[i].removeAttribute('onclick');
      }
    }

    var collapseElements = document.getElementsByAttribute("collapse-on-readonly", "true");
    for (var i=0;i<collapseElements.length;i++) {
      collapseElements[i].setAttribute('collapsed', 'true');
    }
    
  } else {
  
    var enableElements = document.getElementsByAttribute("disable-on-readonly", "true");
    for (var i=0;i<enableElements.length;i++) {
      enableElements[i].removeAttribute('disabled');
      if(enableElements[i].hasAttribute('hyperlink'))
        enableElements[i].setAttribute('class','text-link');
    }

    var collapseElements = document.getElementsByAttribute("collapse-on-readonly", "true");
    for (var i=0;i<collapseElements.length;i++) {
      collapseElements[i].removeAttribute('collapsed');
    }

    // Task completed date
    if (item.completedDate) {
        updateToDoStatus(item.status, item.completedDate.jsDate);
    } else {
        updateToDoStatus(item.status);
    }

    // disable repeat menupopup if this is an occurrence
    var item = window.calendarItem;
    if(item.parentItem != item) {
      disableElement("item-repeat");
      var repeatDetails = document.getElementById("repeat-details");
      var numChilds = repeatDetails.childNodes.length;
      for (var i = 0; i < numChilds; i++) {
        var node = repeatDetails.childNodes[i];
        node.setAttribute('disabled', 'true');
        node.removeAttribute('class');
        node.removeAttribute('onclick');
      }
    }

    // if the item is a proxy occurrence/instance, a few things aren't valid.
    if (item.parentItem != item) {

      setElementValue("item-calendar", "true", "disabled");

      // don't allow to revoke the entrydate of recurring todo's.
      disableElement("todo-has-entrydate");
    }
    
    // don't allow to revoke the entrydate of recurring todo's.
    if(window.recurrenceInfo)
      disableElement("todo-has-entrydate");
    
    // update datetime pickers
    updateDueDate();
    updateEntryDate();

    // update datetime pickers
    updateAllDay();
  }
}

//////////////////////////////////////////////////////////////////////////////
// splitRecurrenceRules
//////////////////////////////////////////////////////////////////////////////

function splitRecurrenceRules(recurrenceInfo)
{
  var ritems = recurrenceInfo.getRecurrenceItems({});

  var rules = [];
  var exceptions = [];

  for each (var r in ritems) {
      if (r.isNegative)
          exceptions.push(r);
      else
          rules.push(r);
  }

  return [rules, exceptions];
}

//////////////////////////////////////////////////////////////////////////////
// editRepeat
//////////////////////////////////////////////////////////////////////////////

function editRepeat()
{
  var args = new Object();
  args.calendarEvent = window.calendarItem;
  args.recurrenceInfo = window.recurrenceInfo;
  args.startTime = gStartTime;
  args.endTime = gEndTime;
  
  var savedWindow = window;
  args.onOk = function(recurrenceInfo) {
      savedWindow.recurrenceInfo = recurrenceInfo;
  };

  window.setCursor("wait");

  // open the dialog modally
  openDialog("chrome://calendar/content/sun-calendar-event-dialog-recurrence.xul", "_blank", "chrome,titlebar,modal,resizable", args);
}

//////////////////////////////////////////////////////////////////////////////
// updateRepeat
//////////////////////////////////////////////////////////////////////////////

// this function is called after the 'repeat pattern' selection has been changed.
// as a consequence we need to create/modify recurrence rules or bring up the
// custom 'repeat pattern'-dialog and modify states of several elements of the
// document (i.e. task entrydate, etc.)
function updateRepeat()
{
  var repeatMenu = document.getElementById("item-repeat");
  var repeatItem = repeatMenu.selectedItem;
  var repeatValue = repeatItem.getAttribute("value");
  
  if (repeatValue == 'none') {
  
    window.recurrenceInfo = null;

    var item = window.calendarItem;
    if (isToDo(item)) {
      enableElement("todo-has-entrydate");
    }
  
  } else if (repeatValue == 'custom') {

    // the user selected custom repeat pattern. we now need to bring
    // up the appropriate dialog in order to let the user specify the
    // new rule. first of all, retrieve the item we want to specify
    // the custom repeat pattern for.
    var item = window.calendarItem;
    
    // if this item is a task, we need to make sure that it has
    // an entry-date, otherwise we can't create a recurrence.
    if (isToDo(item)) {
    
      // automatically check 'has entrydate' if needed.
      if (!getElementValue("todo-has-entrydate", "checked")) {
        setElementValue("todo-has-entrydate", "true", "checked");
        
        // make sure gStartTime is properly initialized
        updateEntryDate();
      }
      
      // disable the checkbox to indicate that we need
      // the entry-date. the 'disabled' state will be
      // revoked if the user turns off the repeat pattern.
      disableElement("todo-has-entrydate");
    }
    
    // retrieve the current recurrence info, we need this
    // to find out whether or not the user really created
    // a new repeat pattern.
    var recurrenceInfo = window.recurrenceInfo;
    
    // now bring up the recurrence dialog.
    editRepeat();

    // we need to address two separate cases here.
    // 1) we need to revoke the selection of the repeat
    //    drop down list in case the user didn't specify
    //    a new repeat pattern (i.e. canceled the dialog)
    // 2) re-enable the 'has entrydate' option in case
    //    we didn't end up with a recurrence rule.
    if (recurrenceInfo == window.recurrenceInfo) {
      repeatMenu.selectedIndex = gLastRepeatSelection;
      if (isToDo(item)) {
        if(!window.recurrenceInfo) {
          enableElement("todo-has-entrydate");
        }
      }
    }
    
  } else {

    var item = window.calendarItem;
    var recurrenceInfo = window.recurrenceInfo;
    if (!recurrenceInfo)
      recurrenceInfo = item.recurrenceInfo;
    if (recurrenceInfo) {
      recurrenceInfo = recurrenceInfo.clone();
      var rrules = splitRecurrenceRules(recurrenceInfo);
      if (rrules[0].length > 0)
          recurrenceInfo.deleteRecurrenceItem(rrules[0][0]);
    } else {
      recurrenceInfo = createRecurrenceInfo();
      recurrenceInfo.item = item;
    }
      
    switch (repeatValue)
    {
      case 'daily':
        var recRule = createRecurrenceRule();
        recRule.type = 'DAILY';
        recRule.interval = 1;
        recRule.count = -1;
        break;
      case 'weekly':
        var recRule = createRecurrenceRule();
        recRule.type = 'WEEKLY';
        recRule.interval = 1;
        recRule.count = -1;
        break;
      case 'every.weekday':
        var recRule = createRecurrenceRule();
        recRule.type = 'DAILY';
        recRule.interval = 1;
        recRule.count = -1;
        var onDays = [ 2,3,4,5,6 ];
        recRule.setComponent("BYDAY", onDays.length, onDays);
        break;
      case 'bi.weekly':
        var recRule = createRecurrenceRule();
        recRule.type = 'WEEKLY';
        recRule.interval = 2;
        recRule.count = -1;
        break;
      case 'monthly':
        var recRule = createRecurrenceRule();
        recRule.type = 'MONTHLY';
        recRule.interval = 1;
        recRule.count = -1;
        break;
      case 'yearly':
        var recRule = createRecurrenceRule();
        recRule.type = 'YEARLY';
        recRule.interval = 1;
        recRule.count = -1;
        break;
    }
    
    recurrenceInfo.insertRecurrenceItemAt(recRule, 0);
    window.recurrenceInfo = recurrenceInfo;

    if (isToDo(item)) {
      if (!getElementValue("todo-has-entrydate", "checked")) {
        setElementValue("todo-has-entrydate", "true", "checked");
      }
      disableElement("todo-has-entrydate");
    }
  }

  gLastRepeatSelection = repeatMenu.selectedIndex;

  updateRepeatDetails();
  updateAccept();
}

//////////////////////////////////////////////////////////////////////////////
// updateToDoStatus
//////////////////////////////////////////////////////////////////////////////

function updateToDoStatus(status,passedInCompletedDate)
{
  // RFC2445 doesn't support completedDates without the todo's status
  // being "COMPLETED", however twiddling the status menulist shouldn't
  // destroy that information at this point (in case you change status
  // back to COMPLETED). When we go to store this VTODO as .ics the
  // date will get lost.

  var completedDate;
  if (passedInCompletedDate) {
      completedDate = passedInCompletedDate;
  } else {
      completedDate = null;
  }

  // remember the original values
  var oldPercentComplete = getElementValue("percent-complete-textbox");
  var oldCompletedDate   = getElementValue("completed-date-picker");

  switch (status) {
  case null:
  case "":
  case "NONE":
      document.getElementById("todo-status").selectedIndex = 0;
      disableElement("percent-complete-textbox");
      disableElement("percent-complete-label");
      break;
  case "CANCELLED":
      document.getElementById("todo-status").selectedIndex = 4;
      disableElement("percent-complete-textbox");
      disableElement("percent-complete-label");
      break;
  case "COMPLETED":
      document.getElementById("todo-status").selectedIndex = 3;
      enableElement("percent-complete-textbox");
      enableElement("percent-complete-label");
      // if there isn't a completedDate, set it to now
      if (!completedDate)
          completedDate = new Date();
      break;
  case "IN-PROCESS":
      document.getElementById("todo-status").selectedIndex = 2;
      disableElement("completed-date-picker");
      enableElement("percent-complete-textbox");
      enableElement("percent-complete-label");
      break;
  case "NEEDS-ACTION":
      document.getElementById("todo-status").selectedIndex = 1;
      enableElement("percent-complete-textbox");
      enableElement("percent-complete-label");
      break;
  }

  if (status == "COMPLETED") {
      setElementValue("percent-complete-textbox", "100");
      setElementValue("completed-date-picker", completedDate);
      enableElement("completed-date-picker");
  } else {
      if (oldPercentComplete != 100) {
          setElementValue("percent-complete-textbox", oldPercentComplete);
      } else {
          setElementValue("percent-complete-textbox", "");
      }
      setElementValue("completed-date-picker", oldCompletedDate);
      disableElement("completed-date-picker");
  }
}

//////////////////////////////////////////////////////////////////////////////
// saveItem
//////////////////////////////////////////////////////////////////////////////

function saveItem()
{
  // if this event isn't mutable, we need to clone it like a sheep
  var originalItem = window.calendarItem;
  var item = (originalItem.isMutable) ? originalItem : originalItem.clone();

  // override item's recurrenceInfo *before* serializing date/time-objects.
  if(!window.isOccurrence)
    item.recurrenceInfo = window.recurrenceInfo;

  // serialize the item
  saveDialog(item);

  // we set the organizer of this item only if
  // it is a stand-alone instance [not an occurrence].
  if(!window.isOccurrence)
    item.organizer = window.organizer;

  // TODO: we set the array of attendees for the new item
  // regardless of it being an occurrence or not. probably
  // this is not correct.
  if(window.attendees) {
    item.removeAllAttendees();
    for each(var attendee in window.attendees) {
       item.addAttendee(attendee);
    }
  }
    
  return item;
}

//////////////////////////////////////////////////////////////////////////////
// onCommandSave
//////////////////////////////////////////////////////////////////////////////

function onCommandSave()
{
  var progress = document.getElementById("statusbar-progress");
  progress.setAttribute("mode","undetermined");

  var originalItem = window.calendarItem;
  var item = saveItem();
  var calendar = document.getElementById("item-calendar").selectedItem.calendar;
  window.onAcceptCallback(item, calendar, originalItem);

  var callback = function func() {
    progress.setAttribute("mode","normal");
  }
  setTimeout(callback, 1000);
  
  item.makeImmutable();
  window.calendarItem = item;
}

//////////////////////////////////////////////////////////////////////////////
// onCommandExit
//////////////////////////////////////////////////////////////////////////////

function onCommandExit()
{
  // the correct way would be to hook 'onCancel' to the
  // 'tryToClose' attribute, but if the user wants to save
  // the changes we're running into trouble since the calendar
  // engine won't exit any longer, which results in dataloss.
  // window.tryToClose = onCancel;
  if(onCancel()) {
    goQuitApplication()
  }
}

//////////////////////////////////////////////////////////////////////////////
// onCommandViewToolbar
//////////////////////////////////////////////////////////////////////////////

function onCommandViewToolbar(aToolbarId, aMenuItemId)
{
  var toolbar = document.getElementById(aToolbarId);
  var menuItem = document.getElementById(aMenuItemId);

  if (!toolbar || !menuItem) return;

  var toolbarCollapsed = toolbar.collapsed;
  
  // toggle the checkbox
  menuItem.setAttribute('checked', toolbarCollapsed);
  
  // toggle visibility of the toolbar
  toolbar.collapsed = !toolbarCollapsed;   

  document.persist(aToolbarId, 'collapsed');
  document.persist(aMenuItemId, 'checked');
}

//////////////////////////////////////////////////////////////////////////////
// onCommandCustomize
//////////////////////////////////////////////////////////////////////////////

function onCommandCustomize()
{
  var id = "event-toolbox";
  var aToolbarId = 'event-toolbar';
  var aMenuItemId = 'menu-view-toolbar';
  var toolbar = document.getElementById(aToolbarId);
  var toolbarCollapsed = toolbar.collapsed;
  if(toolbarCollapsed) {
    onCommandViewToolbar(aToolbarId,aMenuItemId);
  }
  
  window.openDialog("chrome://calendar/content/sun-calendar-customize-toolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById(id));
}

//////////////////////////////////////////////////////////////////////////////
// onAboutDialog
//////////////////////////////////////////////////////////////////////////////

function onAboutDialog()
{
  openDialog("chrome://messenger/content/aboutDialog.xul", "About", "modal,centerscreen,chrome,resizable=no");
}

//////////////////////////////////////////////////////////////////////////////
// openRegionURL
//////////////////////////////////////////////////////////////////////////////

/**
 * Opens region specific web pages for the application like the release notes, the help site, etc. 
 *   aResourceName --> the string resource ID in region.properties to load. 
 */
function openRegionURL(aResourceName)
{
  var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                          .getService(Components.interfaces.nsIXULAppInfo);
  try {
    var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].getService(Components.interfaces.nsIStringBundleService);
    var regionBundle = strBundleService.createBundle("chrome://messenger-region/locale/region.properties");
    // the release notes are special and need to be formatted with the app version
    var urlToOpen;
    if (aResourceName == "releaseNotesURL")
      urlToOpen = regionBundle.formatStringFromName(aResourceName, [appInfo.version], 1);
    else
      urlToOpen = regionBundle.GetStringFromName(aResourceName);
      
    var uri = Components.classes["@mozilla.org/network/io-service;1"]
              .getService(Components.interfaces.nsIIOService)
              .newURI(urlToOpen, null, null);

    var protocolSvc = Components.classes["@mozilla.org/uriloader/external-protocol-service;1"]
                      .getService(Components.interfaces.nsIExternalProtocolService);
    protocolSvc.loadUrl(uri);
  } catch (ex) {}
}

//////////////////////////////////////////////////////////////////////////////
// editStartTimezone
//////////////////////////////////////////////////////////////////////////////

function editStartTimezone()
{
  var tzStart = document.getElementById("timezone-starttime");
  if(tzStart.hasAttribute("disabled"))
    return;

  var args = new Object();
  args.time = gStartTime.getInTimezone(gStartTimezone);
  args.onOk = function(datetime) {

    var equalTimezones = false;
    if(gStartTimezone && gEndTimezone) {
      if(gStartTimezone == gEndTimezone) {
        equalTimezones = true;
      }
    }

    gStartTimezone = datetime.timezone;
    if(equalTimezones)
      gEndTimezone = datetime.timezone;

    updateDateTime();
  };

  // open the dialog modally
  openDialog("chrome://calendar/content/sun-calendar-event-dialog-timezone.xul", "_blank", "chrome,titlebar,modal,resizable", args);
}

//////////////////////////////////////////////////////////////////////////////
// editEndTimezone
//////////////////////////////////////////////////////////////////////////////

function editEndTimezone()
{
  var tzStart = document.getElementById("timezone-endtime");
  if(tzStart.hasAttribute("disabled"))
    return;

  var args = new Object();
  args.time = gEndTime.getInTimezone(gEndTimezone);
  args.onOk = function(datetime) {
    
    var equalTimezones = false;
    if(gStartTimezone && gEndTimezone) {
      if(gStartTimezone == gEndTimezone) {
        equalTimezones = true;
      }
    }
    
    if(equalTimezones)
      gStartTimezone = datetime.timezone;
    gEndTimezone = datetime.timezone;
    
    updateDateTime();
  };

  // open the dialog modally
  openDialog("chrome://calendar/content/sun-calendar-event-dialog-timezone.xul", "_blank", "chrome,titlebar,modal,resizable", args);
}

//////////////////////////////////////////////////////////////////////////////
// updateDateTime
//////////////////////////////////////////////////////////////////////////////

// this function initializes the following controls:
// - 'event-starttime'
// - 'event-endtime'
// - 'event-all-day'
// - 'todo-has-entrydate'
// - 'todo-entrydate'
// - 'todo-has-duedate'
// - 'todo-duedate'
// the date/time-objects are either displayed in their repective
// timezone or in the default timezone. this decision is based
// on whether or not 'menu-options-timezone' is checked.
// the necessary information is taken from the following variables:
// - 'gStartTime'
// - 'gEndTime'
// - 'window.calendarItem' (used to decide about event/task)
function updateDateTime()
{
  //gConsoleService.logStringMessage(
    //"### updateDateTime()\n"+
    //"startTime: "+gStartTime.toString()+" "+gStartTime.isDate+"\n"+
    //"endTime: "+gEndTime.toString()+" "+gStartTime.isDate+"\n");

  gIgnoreUpdate = true;

  var item = window.calendarItem;
  var menuItem = document.getElementById('menu-options-timezone');

  // convert to default timezone if the timezone option
  // is *not* checked, otherwise keep the specific timezone
  // and display the labels in order to modify the timezone.
  if(menuItem.getAttribute('checked') == 'true') {

    if (isEvent(item)) {

      var startTime = gStartTime.getInTimezone(gStartTimezone);
      var endTime = gEndTime.getInTimezone(gEndTimezone);

      if (startTime.isDate) {
        setElementValue("event-all-day", true, "checked");
      } else {
        setElementValue("event-all-day", false, "checked");
      }

      // in the case where the timezones are different but
      // the timezone of the endtime is "UTC", we convert
      // the endtime into the timezone of the starttime.
      if(startTime && endTime) {
        if(startTime.timezone != endTime.timezone) {
          if(endTime.timezone == "UTC") {
            endTime = endTime.getInTimezone(startTime.timezone);
          }
        }
      }

      // before feeding the date/time value into the control we need
      // to set the timezone to 'floating' in order to avoid the
      // automatic conversion back into the OS timezone.
      startTime.timezone = "floating";
      endTime.timezone = "floating";

      setElementValue("event-starttime",startTime.jsDate);
      setElementValue("event-endtime",endTime.jsDate);
    }    
    
    if (isToDo(item)) {
    
      var startTime = gStartTime ? gStartTime.getInTimezone(gStartTimezone) : null;
      var endTime = gEndTime ? gEndTime.getInTimezone(gEndTimezone) : null;
    
      var hasEntryDate = (startTime != null);
      var hasDueDate = (endTime != null);

      if(hasEntryDate) {
        setElementValue("todo-has-entrydate", hasEntryDate, "checked");
        startTime.timezone = "floating";
        setElementValue("todo-entrydate",startTime.jsDate);
      } else if(hasDueDate) {
        endTime.timezone = "floating";
        setElementValue("todo-entrydate",endTime.jsDate);
      }
      
      if(hasDueDate) {
        setElementValue("todo-has-duedate", hasDueDate, "checked");
        endTime.timezone = "floating";
        setElementValue("todo-duedate",endTime.jsDate);
      } else if(hasEntryDate) {
        startTime.timezone = "floating";
        setElementValue("todo-duedate",startTime.jsDate);
      }
    }  
      
  } else {

    var kDefaultTimezone = calendarDefaultTimezone();

    if (isEvent(item)) {

      var startTime = gStartTime.getInTimezone(kDefaultTimezone);
      var endTime = gEndTime.getInTimezone(kDefaultTimezone);

      if (startTime.isDate) {
        setElementValue("event-all-day", true, "checked");
      } else {
        setElementValue("event-all-day", false, "checked");
      }

      // before feeding the date/time value into the control we need
      // to set the timezone to 'floating' in order to avoid the
      // automatic conversion back into the OS timezone.
      startTime.timezone = "floating";
      endTime.timezone = "floating";

      setElementValue("event-starttime",startTime.jsDate);
      setElementValue("event-endtime",endTime.jsDate);
    }

    if (isToDo(item)) {
    
      var startTime = gStartTime ? gStartTime.getInTimezone(kDefaultTimezone) : null;
      var endTime = gEndTime ? gEndTime.getInTimezone(kDefaultTimezone) : null;
    
      var hasEntryDate = (startTime != null);
      var hasDueDate = (endTime != null);

      if(hasEntryDate) {
        setElementValue("todo-has-entrydate", hasEntryDate, "checked");
        startTime.timezone = "floating";
        setElementValue("todo-entrydate",startTime.jsDate);
      } else if(hasDueDate) {
        endTime.timezone = "floating";
        setElementValue("todo-entrydate",endTime.jsDate);
      }
      
      if(hasDueDate) {
        setElementValue("todo-has-duedate", hasDueDate, "checked");
        endTime.timezone = "floating";
        setElementValue("todo-duedate",endTime.jsDate);
      } else if(hasEntryDate) {
        startTime.timezone = "floating";
        setElementValue("todo-duedate",startTime.jsDate);
      }
    }
  }
  
  updateTimezone();
  updateAllDay();
  
  gIgnoreUpdate = false;
}

//////////////////////////////////////////////////////////////////////////////
// updateTimezone
//////////////////////////////////////////////////////////////////////////////

// this function initializes the following controls:
// - 'timezone-starttime'
// - 'timezone-endtime'
// the timezone-links show the corrosponding names of the
// start/end times. if 'menu-options-timezone' is not checked
// the links will be collapsed.
function updateTimezone()
{
  //gConsoleService.logStringMessage("### updateTimezone()");
  var menuItem = document.getElementById('menu-options-timezone');

  // convert to default timezone if the timezone option
  // is *not* checked, otherwise keep the specific timezone
  // and display the labels in order to modify the timezone.
  if(menuItem.getAttribute('checked') == 'true') {

    var startTimezone = gStartTimezone;
    var endTimezone = gEndTimezone;

    var equalTimezones = false;
    if(startTimezone && endTimezone) {
      if(startTimezone == endTimezone || endTimezone == "UTC") {
        equalTimezones = true;
      }
    }

    var tzStart = document.getElementById('timezone-starttime');
    var tzEnd = document.getElementById('timezone-endtime');
    
    if(startTimezone != null) {
      tzStart.removeAttribute('collapsed');
      tzStart.value = timezoneString(startTimezone);
      if(gIsReadOnly) {
        tzStart.removeAttribute('class');
        tzStart.removeAttribute('onclick');
        tzStart.setAttribute('disabled', 'true');
      }
    } else {
      tzStart.setAttribute('collapsed','true');
    }
  
    // we never display the second timezone if both are equal
    if(endTimezone != null && !equalTimezones) {  
      tzEnd.removeAttribute('collapsed');
      tzEnd.value = timezoneString(endTimezone);
      if(gIsReadOnly) {
        tzEnd.removeAttribute('class');
        tzEnd.removeAttribute('onclick');
        tzEnd.setAttribute('disabled', 'true');
      }
    } else {
      tzEnd.setAttribute('collapsed','true');
    }
  
  } else {

    document.getElementById('timezone-starttime').setAttribute('collapsed','true');
    document.getElementById('timezone-endtime').setAttribute('collapsed','true');
  }
}

//////////////////////////////////////////////////////////////////////////////
// updateDocument
//////////////////////////////////////////////////////////////////////////////

function updateDocument()
{
  var documentRow = document.getElementById("document-row");
  if(!gURL || gURL == "") {
    documentRow.setAttribute('collapsed','true');
  } else {
    documentRow.removeAttribute('collapsed');
    var documentLink = document.getElementById("document-link");
    var callback = function func() {
      documentLink.setAttribute('value',gURL);
    }
    setTimeout(callback, 1);
  }
}

//////////////////////////////////////////////////////////////////////////////
// browseDocument
//////////////////////////////////////////////////////////////////////////////

function browseDocument()
{
  launchBrowser(gURL);
}

//////////////////////////////////////////////////////////////////////////////
// updateAttendees
//////////////////////////////////////////////////////////////////////////////

function updateAttendees()
{
  var attendeeRow = document.getElementById("attendee-row");
  if(!window.attendees || !window.attendees.length) {
    attendeeRow.setAttribute('collapsed','true');
  } else {
    attendeeRow.removeAttribute('collapsed');
    var attendeeNames = "";
    var numAttendees = window.attendees.length;
    for(var i=0; i<numAttendees; i++) {
      var attendee = window.attendees[i];
      if(attendee.commonName && attendee.commonName.length) {
        attendeeNames += attendee.commonName;
      } else if(attendee.id && attendee.id.length) {
        var email = attendee.id;
        if (email.indexOf("mailto:") == 0)
          email = email.split("mailto:")[1]
        attendeeNames += email;
      } else {
        continue;
      }
      if(i+1 < numAttendees)
        attendeeNames += ',';
    }
    var attendeeList = document.getElementById("attendee-list");
    var callback = function func() {
      attendeeList.setAttribute('value',attendeeNames);
    }
    setTimeout(callback, 1);
  }
}

//////////////////////////////////////////////////////////////////////////////
// updateRepeatDetails
//////////////////////////////////////////////////////////////////////////////

function updateRepeatDetails()
{
  // find relevant elements in the document
  var itemRepeat = document.getElementById("item-repeat");
  var repeatDetails = document.getElementById("repeat-details");
  
  // first of all collapse the details text. if we fail to
  // create a details string, we simply don't show anything.
  repeatDetails.setAttribute("collapsed","true");
  
  // don't try to show the details text
  // for anything but a custom recurrence rule.
  if(itemRepeat.value == "custom") {
  
    // we currently don't support tasks.
    var item = window.calendarItem;
    if (isEvent(item)) {
    
      // retrieve a valid recurrence rule from the currently
      // set recurrence info. bail out if there's more
      // than a single rule or something other than a rule.
      var recurrenceInfo = window.recurrenceInfo;
      if (recurrenceInfo) {
        recurrenceInfo = recurrenceInfo.clone();
        var rrules = splitRecurrenceRules(recurrenceInfo);
        if (rrules[0].length == 1) {
          var rule = rrules[0][0];
          if(rule instanceof Ci.calIRecurrenceRule) {

            // currently we don't allow for any BYxxx-rules.
            if (!checkRecurrenceRule(rule,['BYSECOND',
                                           'BYMINUTE',
                                           //'BYDAY',
                                           'BYHOUR',
                                           //'BYMONTHDAY',
                                           'BYYEARDAY',
                                           'BYWEEKNO',
                                           //'BYMONTH',
                                           'BYSETPOS'])) {
                                           
              var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                  .getService(Components.interfaces.nsIStringBundleService);
              var props = sbs.createBundle("chrome://calendar/locale/sun-calendar-event-dialog.properties");

              var abs = function(v) {
                return v >= 0 ? v : -v;
              }
              var day_of_week = function(day) {
                return abs(day)%8;
              }
              var day_position = function(day) {
                var dow = day_of_week(day);
                return (abs(day)-dow)/8 * ((day<0)?-1:1);
              }

              var ruleString = "???";
              if(rule.type == 'DAILY') {
                if (checkRecurrenceRule(rule,['BYDAY'])) {
                  var days = rule.getComponent("BYDAY",{});
                  var weekdays = [ 2,3,4,5,6 ];
                  if(weekdays.length == days.length) {
                    for(var i=0; i<weekdays.length; i++) {
                      if(weekdays[i] != days[i])
                        break;
                    }
                    if(i == weekdays.length)
                      ruleString = props.GetStringFromName('repeatDetailsRuleDaily4');
                  }
                } else {
                  if(rule.interval == 1) {
                    ruleString = props.GetStringFromName('repeatDetailsRuleDaily1');
                  } else if(rule.interval == 2) {
                    ruleString = props.GetStringFromName('repeatDetailsRuleDaily2');
                  } else {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleDaily3', [ rule.interval ], 1);
                  }
                }
              } else if(rule.type == 'WEEKLY') {

                // weekly recurrence, currently we
                // support a single 'BYDAY'-rule only.
                if (checkRecurrenceRule(rule,['BYDAY'])) {

                  // create a string like 'Monday, Tuesday and Wednesday'
                  var days = rule.getComponent("BYDAY", {});
                  var weekdays = "";
                  for(var i=0; i<days.length; i++) {
                    weekdays += props.GetStringFromName('repeatDetailsDay'+days[i]);
                    if(days.length > 1 && i == (days.length-2)) {
                      weekdays += ' '+props.GetStringFromName('repeatDetailsAnd')+' ';
                    } else if(i < days.length-1) {
                      weekdays += ', ';
                    }
                  }
                  
                  // now decorate this with 'every other week, etc'.
                  if(rule.interval == 1) {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleWeekly1', [ weekdays ], 1);
                  } else if(rule.interval == 2) {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleWeekly2', [ weekdays ], 1);
                  } else {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleWeekly3', [ rule.interval, weekdays ], 2);
                  }
                }              
              
              } else if(rule.type == 'MONTHLY') {

                if (checkRecurrenceRule(rule,['BYDAY'])) {
                
                  var component = rule.getComponent("BYDAY",{});
                  var byday = component[0];
                  var ordinal_string = props.GetStringFromName('repeatDetailsOrdinal'+day_position(byday));
                  var day_string = props.GetStringFromName('repeatDetailsDay'+day_of_week(byday));
                  
                  if(rule.interval == 1) {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleMonthly1', [ ordinal_string, day_string ], 2);
                  } else if(rule.interval == 2) {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleMonthly2', [ ordinal_string, day_string ], 2);
                  } else {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleMonthly3', [ ordinal_string, day_string, rule.interval ], 3);
                  }
                
                } else if (checkRecurrenceRule(rule,['BYMONTHDAY'])) {
                  
                  var component = rule.getComponent("BYMONTHDAY",{});
                  
                  var day_string = "";
                  for(var i=0; i<component.length; i++) {
                    day_string += component[i];
                    if(component.length > 1 && i == (component.length-2)) {
                      day_string += ' '+props.GetStringFromName('repeatDetailsAnd')+' ';
                    } else if(i < component.length-1) {
                      day_string += ', ';
                    }
                  }

                  if(rule.interval == 1) {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleMonthly4', [ day_string ], 1);
                  } else if(rule.interval == 2) {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleMonthly5', [ day_string ], 1);
                  } else {
                    ruleString = props.formatStringFromName(
                      'repeatDetailsRuleMonthly6', [ day_string, rule.interval ], 2);
                  }
                }
              
              } else if(rule.type == 'YEARLY') {
              
                if (checkRecurrenceRule(rule,['BYMONTH']) &&
                    checkRecurrenceRule(rule,['BYMONTHDAY'])) {
                    
                  bymonth = rule.getComponent("BYMONTH",{});
                  bymonthday = rule.getComponent("BYMONTHDAY",{});
                  
                  if(bymonth.length == 1) {
                    if(bymonthday.length == 1) {

                      var month_string = props.GetStringFromName('repeatDetailsMonth'+bymonth[0]);

                      if(rule.interval == 1) {
                        ruleString = props.formatStringFromName(
                          'repeatDetailsRuleYearly1', [ month_string, bymonthday[0] ], 2);
                      } else if(rule.interval == 2) {
                        ruleString = props.formatStringFromName(
                          'repeatDetailsRuleYearly2', [ month_string, bymonthday[0] ], 2);
                      } else {
                        ruleString = props.formatStringFromName(
                          'repeatDetailsRuleYearly3', [ month_string, bymonthday[0], rule.interval ], 3);
                      }
                    }
                  }
                    
                } else if(checkRecurrenceRule(rule,['BYMONTH']) &&
                          checkRecurrenceRule(rule,['BYDAY'])) {
                          
                  bymonth = rule.getComponent("BYMONTH",{});
                  byday = rule.getComponent("BYDAY",{});

                  if(bymonth.length == 1) {
                    if(byday.length == 1) {

                      var month_string = props.GetStringFromName('repeatDetailsMonth'+bymonth[0]);
                      var ordinal_string = props.GetStringFromName('repeatDetailsOrdinal'+day_position(byday[0]));
                      var day_string = props.GetStringFromName('repeatDetailsDay'+day_of_week(byday[0]));
                      
                      if(rule.interval == 1) {
                        ruleString = props.formatStringFromName(
                          'repeatDetailsRuleYearly4', [ ordinal_string, day_string, month_string ], 3);
                      } else if(rule.interval == 2) {
                        ruleString = props.formatStringFromName(
                          'repeatDetailsRuleYearly5', [ ordinal_string, day_string, month_string ], 3);
                      } else {
                        ruleString = props.formatStringFromName(
                          'repeatDetailsRuleYearly6', [ ordinal_string, day_string, month_string, rule.interval ], 4);
                      }
                    }
                  }
                }
              }

              var kDefaultTimezone = calendarDefaultTimezone();
              var startDate = jsDateToDateTime(getElementValue("event-starttime"));
              var endDate = jsDateToDateTime(getElementValue("event-endtime"));
              startDate = startDate.getInTimezone(kDefaultTimezone);
              endDate = endDate.getInTimezone(kDefaultTimezone);
              var isAllDay = getElementValue("event-all-day", "checked");
                        
              var dateFormatter = Components.classes["@mozilla.org/calendar/datetime-formatter;1"]
                                            .getService(Components.interfaces.calIDateTimeFormatter);

              var detailsString;
              if(isAllDay) {
                if(rule.isFinite) {
                  if(rule.isByCount) {
                    detailsString = props.formatStringFromName(
                      'repeatDetailsCountAllDay',
                      [ ruleString,
                        dateFormatter.formatDateShort(startDate),
                        rule.count ], 3);
                  } else {
                    var untilDate = rule.endDate.getInTimezone(kDefaultTimezone);
                    detailsString = props.formatStringFromName(
                      'repeatDetailsUntilAllDay',
                      [ ruleString,
                        dateFormatter.formatDateShort(startDate),
                        dateFormatter.formatDateShort(untilDate) ], 3);
                  }
                } else {
                  detailsString = props.formatStringFromName(
                    'repeatDetailsInfiniteAllDay',
                    [ ruleString,
                      dateFormatter.formatDateShort(startDate) ], 2);
                }
              } else {
                if(rule.isFinite) {
                  if(rule.isByCount) {
                    detailsString = props.formatStringFromName(
                      'repeatDetailsCount',
                      [ ruleString,
                        dateFormatter.formatDateShort(startDate),
                        rule.count,
                        dateFormatter.formatTime(startDate),
                        dateFormatter.formatTime(endDate) ], 5);
                  } else {
                    var untilDate = rule.endDate.getInTimezone(kDefaultTimezone);
                    detailsString = props.formatStringFromName(
                      'repeatDetailsUntil',
                      [ ruleString,
                        dateFormatter.formatDateShort(startDate),
                        dateFormatter.formatDateShort(untilDate),
                        dateFormatter.formatTime(startDate),
                        dateFormatter.formatTime(endDate) ], 5);
                  }
                } else {
                  detailsString = props.formatStringFromName(
                    'repeatDetailsInfinite',
                    [ ruleString,
                      dateFormatter.formatDateShort(startDate),
                      dateFormatter.formatTime(startDate),
                      dateFormatter.formatTime(endDate) ], 4);
                }
              }

              if(detailsString) {
                var lines = detailsString.split("\n");
                repeatDetails.removeAttribute("collapsed");
                while(repeatDetails.childNodes.length > lines.length)
                  repeatDetails.removeChild(repeatDetails.lastChild);
                var numChilds = repeatDetails.childNodes.length;
                for (var i = 0; i < lines.length; i++) {
                  if(i >= numChilds) {
                    var newNode = repeatDetails.childNodes[0].cloneNode(true);
                    repeatDetails.appendChild(newNode);
                  }
                  repeatDetails.childNodes[i].value = lines[i];
                }
              }
            }
          }
        }
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// End of file
//////////////////////////////////////////////////////////////////////////////
