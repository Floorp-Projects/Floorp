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

  // set the current date/time for the duedate control.
  // this is just a placeholder until we support absolute reminders.
  var duedate = document.getElementById("reminder-duetime");
  duedate.value = new Date();

  // load the reminders from preferences
  var reminders = loadReminders();
  
  // check if a possibly incoming reminder object matches
  // the ones we already know about...
  var incoming = args.reminder;
  var selectedIndex = -1;
  if(incoming) {
    for (var i=0; i<reminders.length; i++) {
      var reminder = reminders[i];
      if(reminder.relation == incoming.relation &&
         reminder.length == incoming.length &&
         reminder.unit == incoming.unit) {
        selectedIndex = i;
        break;
      }
    }

    // if we didn't find a match, we append the unknown
    // reminder to the array of custom reminders.
    if(selectedIndex < 0) {
      reminders.push(incoming);
      selectedIndex = reminders.length-1;
    }
  }
  
  var listbox = document.getElementById("reminder-listbox");
  while(listbox.childNodes.length > reminders.length)
    listbox.removeChild(listbox.lastChild);
  var numChilds = listbox.childNodes.length;
  for (var i=0; i<reminders.length; i++) {
    if(i >= numChilds) {
      var newNode = listbox.childNodes[0].cloneNode(true);
      listbox.appendChild(newNode);
    }
    var node = listbox.childNodes[i];
    var reminder = reminders[i];
    var details = stringFromReminderObject(reminder);
    node.setAttribute('label',details);
    node.reminder = reminder;
  }

  if(selectedIndex >= 0)
    listbox.selectedIndex = selectedIndex;

  opener.setCursor("auto");
}

//////////////////////////////////////////////////////////////////////////////
// stringFromReminderObject
//////////////////////////////////////////////////////////////////////////////

function stringFromReminderObject(reminder) {
  
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

  var result = props.formatStringFromName(
    'reminderCustomTitle',
    [ reminder.length,
      unitString,
      relationString ], 3);

  return result;
}

//////////////////////////////////////////////////////////////////////////////
// loadReminders
//////////////////////////////////////////////////////////////////////////////

function loadReminders()
{
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefBranch = prefService.getBranch("calendar.reminder.");

  var pref = "length=15;unit=minutes;relation=START,length=3;unit=hours;relation=START";
  try {
    var newPref = prefBranch.getCharPref("custom");
    if(newPref && newPref != "")
      pref = newPref;
  } catch (ex) {}

  var reminderArray = [];
  var reminders = pref.split(',');
  for each(var reminder in reminders) {
    if(reminder.indexOf(';') >= 0) {
      var object = {};
      var attributes = reminder.split(';');
      for each(var attribute in attributes) {
        var index = attribute.indexOf('=');
        if(index > 0) {
          var key = attribute.substring(0,index);
          var value = attribute.substring(index+1,attribute.length);
          object[key] = value;
        }
      }
      reminderArray.push(object);
    }
  }

  return reminderArray;
}

//////////////////////////////////////////////////////////////////////////////
// saveReminders
//////////////////////////////////////////////////////////////////////////////

function saveReminders(reminderArray)
{
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefBranch = prefService.getBranch("calendar.reminder.");

  var result = "";
  for each(var reminder in reminderArray) {
    for(var key in reminder) {
      result += key+'='+reminder[key]+';';
    }
    if(result.length && result[result.length-1] == ';')
      result = result.substr(0,result.length-1);
    result += ',';
  }
  if(result.length && result[result.length-1] == ',')
    result = result.substr(0,result.length-1);

  prefBranch.setCharPref("custom",result);
}

//////////////////////////////////////////////////////////////////////////////
// onReminderSelected
//////////////////////////////////////////////////////////////////////////////

function onReminderSelected()
{
  var length = document.getElementById("reminder-length");
  var unit = document.getElementById("reminder-unit");
  var relation = document.getElementById("reminder-relation");
  
  var listbox = document.getElementById("reminder-listbox");
  var listitem = listbox.selectedItem;

  if(listitem) {

    var reminder = listitem.reminder;
    length.value = reminder.length;
    unit.value = reminder.unit;
    relation.value = reminder.relation;
  }
  else {
  }
}

//////////////////////////////////////////////////////////////////////////////
// updateReminderLength
//////////////////////////////////////////////////////////////////////////////

function updateReminderLength(event) {

  validateIntegers(event);
  updateReminder();
}

//////////////////////////////////////////////////////////////////////////////
// updateReminder
//////////////////////////////////////////////////////////////////////////////

function updateReminder() {

  var length = document.getElementById("reminder-length");
  var unit = document.getElementById("reminder-unit");
  var relation = document.getElementById("reminder-relation");

  var listbox = document.getElementById("reminder-listbox");
  var listitem = listbox.selectedItem;
  var reminder = listitem.reminder;
  
  reminder.length = length.value;
  reminder.unit = unit.value;
  reminder.relation = relation.value;

  var details = stringFromReminderObject(reminder);
  listitem.setAttribute('label',details);
}

//////////////////////////////////////////////////////////////////////////////
// onNewReminder
//////////////////////////////////////////////////////////////////////////////

function onNewReminder() {

  var listbox = document.getElementById("reminder-listbox");
  var listitem = listbox.selectedItem;
  var reminder = listitem.reminder;
  listbox.clearSelection();
  var newNode = listitem.cloneNode(true);
  var newReminder = {}
  newReminder.length = reminder.length;
  newReminder.unit = reminder.unit;
  newReminder.relation = reminder.relation;
  newNode.reminder = newReminder;
  listbox.appendChild(newNode);
  listbox.selectItem(newNode);

  var button = document.getElementById("reminder-delete");
  if(listbox.childNodes.length > 1) {
    button.removeAttribute('disabled');
  } else {
    button.setAttribute('disabled','true');
  }
}

//////////////////////////////////////////////////////////////////////////////
// onDeleteReminder
//////////////////////////////////////////////////////////////////////////////

function onDeleteReminder() {

  var listbox = document.getElementById("reminder-listbox");
  var listitem = listbox.selectedItem;
  var selectitem = listitem.nextSibling;
  if (!selectitem)
    selectitem = listitem.previousSibling;
  listbox.clearSelection();
  listbox.removeChild(listitem);
  listbox.selectItem(selectitem);

  var button = document.getElementById("reminder-delete");
  if(listbox.childNodes.length > 1) {
    button.removeAttribute('disabled');
  } else {
    button.setAttribute('disabled','true');
  }
}

//////////////////////////////////////////////////////////////////////////////
// onAccept
//////////////////////////////////////////////////////////////////////////////

function onAccept()
{
  var array = [];
  var listbox = document.getElementById("reminder-listbox");
  var numChilds = listbox.childNodes.length;
  for (var i = 0; i < numChilds; i++) {
    var item = listbox.childNodes[i];
    array.push(item.reminder);
  }
  saveReminders(array);

  var listitem = listbox.selectedItem;
  window.onAcceptCallback(listitem.reminder);

  return true;
}

//////////////////////////////////////////////////////////////////////////////
// onCancel
//////////////////////////////////////////////////////////////////////////////

function onCancel()
{
}

//////////////////////////////////////////////////////////////////////////////
// End of file
//////////////////////////////////////////////////////////////////////////////
