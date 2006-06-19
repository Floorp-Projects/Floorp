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
 *                 Karl Guertin <grayrest@grayrest.com> 
 *                 Mike Norton <xor@ivwnet.com>
 *                 ArentJan Banck <ajbanck@planet.nl> 
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Matthew Willis <mattwillis@gmail.com>
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

/***** calendar
* AUTHOR
*   Garth Smedley
*
* NOTES
*   Code for the calendar.
*
*   What is in this file:
*     - Global variables and functions - Called directly from the XUL
*     - Several classes:
*  
* IMPLEMENTATION NOTES 
*
**********
*/

// single global instance of CalendarWindow
var gCalendarWindow;

/*-----------------------------------------------------------------
*  G L O B A L     C A L E N D A R      F U N C T I O N S
*/

/** 
* Called from calendar.xul window onload.
*/

function calendarInit() 
{
   // set up the CalendarWindow instance
   
   gCalendarWindow = new CalendarWindow();

   // Set up the checkbox variables.  Do not use the typical change*() functions
   // because those will actually toggle the current value.
   if (document.getElementById("toggle_workdays_only").getAttribute("checked")
       == 'true') {
       var deck = document.getElementById("view-deck")
       for each (view in deck.childNodes) {
           view.workdaysOnly = true;
       }
       deck.selectedPanel.goToDay(deck.selectedPanel.selectedDay);
   }

   // tasksInView is true by default
   if (document.getElementById("toggle_tasks_in_view").getAttribute("checked")
       != 'true') {
       var deck = document.getElementById("view-deck")
       for each (view in deck.childNodes) {
           view.tasksInView = false;
       }
       deck.selectedPanel.goToDay(deck.selectedPanel.selectedDay);
   }

   // set up the unifinder
   
   prepareCalendarUnifinder();

   prepareCalendarToDoUnifinder();
   
   update_date();

   checkForMailNews();

   initCalendarManager();

   // fire up the alarm service
   var alarmSvc = Components.classes["@mozilla.org/calendar/alarm-service;1"]
                  .getService(Components.interfaces.calIAlarmService);
   alarmSvc.timezone = calendarDefaultTimezone();
   alarmSvc.startup();

   if (("arguments" in window) && (window.arguments.length) &&
       (typeof(window.arguments[0]) == "object") &&
       ("channel" in window.arguments[0]) ) {
       gCalendarWindow.calendarManager.checkCalendarURL( 
           window.arguments[0].channel );
   }

   // a bit of a hack since the menulist doesn't remember the selected value
   var value = document.getElementById( 'event-filter-menulist' ).value;
   document.getElementById( 'event-filter-menulist' ).selectedItem = 
       document.getElementById( 'event-filter-'+value );

   var toolbox = document.getElementById("calendar-toolbox");
   toolbox.customizeDone = CalendarToolboxCustomizeDone;

   document.getElementById("view-deck")
           .addEventListener("dayselect", observeViewDaySelect, false);
}

// Set the date and time on the clock and set up a timeout to refresh the clock when the 
// next minute ticks over

function update_date()
{
   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   gCalendarWindow.currentView.hiliteTodaysDate();

   refreshEventTree();

   // Is an nsITimer/callback extreme overkill here? Yes, but it's necessary to
   // workaround bug 291386.  If we don't, we stand a decent chance of getting
   // stuck in an infinite loop.
   var udCallback = {
       notify: function(timer) {
           update_date();
       }
   };

   var timer = Components.classes["@mozilla.org/timer;1"]
                         .createInstance(Components.interfaces.nsITimer);
   timer.initWithCallback(udCallback, milliSecsTillTomorrow, timer.TYPE_ONE_SHOT);
}

/** 
* Called from calendar.xul window onunload.
*/

function calendarFinish()
{
   // Workaround to make the selected tab persist. See bug 249552.
   var tabbox = document.getElementById("tablist");
   tabbox.setAttribute("selectedIndex", tabbox.selectedIndex);

   finishCalendarUnifinder();
   
   finishCalendarToDoUnifinder();

   finishCalendarManager();
}

function launchPreferences()
{
    var applicationName="";
    if (Components.classes["@mozilla.org/xre/app-info;1"]) {
        var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                      .getService(Components.interfaces.nsIXULAppInfo);
        applicationName = appInfo.name;
    }
    if (applicationName == "SeaMonkey" || applicationName == "")
        goPreferences( "calendarPanel", "chrome://calendar/content/pref/calendarPref.xul", "calendarPanel" );
    else
        window.openDialog("chrome://calendar/content/pref/prefBird.xul", "PrefWindow", "chrome,titlebar,resizable,modal");
}

function newCalendarDialog()
{
    openCalendarWizard();
}

function editCalendarDialog(event)
{
    openCalendarProperties(document.popupNode.calendar, null);
}

function calendarListboxDoubleClick(event) {
    if(event.target.calendar)
        openCalendarProperties(event.target.calendar, null);
    else
        openCalendarWizard();
}

function checkCalListTarget() {
    if(!document.popupNode.calendar) {
        document.getElementById("calpopup-edit").setAttribute("disabled", "true");
        document.getElementById("calpopup-delete").setAttribute("disabled", "true");
        document.getElementById("calpopup-publish").setAttribute("disabled", "true");
    }
    else {
        document.getElementById("calpopup-edit").removeAttribute("disabled");
        document.getElementById("calpopup-delete").removeAttribute("disabled");
        document.getElementById("calpopup-publish").removeAttribute("disabled");
    }
}

function deleteCalendar(event)
{
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService); 

    var result = {}; 
    var calendarBundle = document.getElementById("bundle_calendar");
    var calendar = document.popupNode.calendar;
    var ok = promptService.confirm(
        window,
        calendarBundle.getString("unsubscribeCalendarTitle"),
        calendarBundle.getFormattedString("unsubscribeCalendarMessage",[calendar.name]),
        result);
   
    if (ok) {
        getDisplayComposite().removeCalendar(calendar.uri);
        var calMgr = getCalendarManager();
        calMgr.unregisterCalendar(calendar);
        // Delete file?
        //calMgr.deleteCalendar(cal);
    }
}

/** 
* Defaults null start/end date based on selected date in current view.
* Defaults calendarFile to the selected calendar file.
* Calls editNewEvent. 
*/

function newEvent(startDate, endDate, allDay)
{
   // create a new event to be edited and added
   var calendarEvent = createEvent();

   if (!startDate) {
       startDate = gCalendarWindow.currentView.getNewEventDate();
   }

   calendarEvent.startDate.jsDate = startDate;

   if (!endDate) {
       var pb2 = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch2);
       var MinutesToAddOn = pb2.getIntPref("calendar.event.defaultlength");
       
       endDate = new Date(startDate);
       endDate.setMinutes(endDate.getMinutes() + MinutesToAddOn);
   }

   calendarEvent.endDate.jsDate = endDate

   setDefaultAlarmValues(calendarEvent);

   if (allDay)
       calendarEvent.startDate.isDate = true;

   var calendar = getSelectedCalendarOrNull();

   createEventWithDialog(calendar, null, null, null, calendarEvent);
}

/*
* Defaults null start/due date to the no_date date.
* Defaults calendarFile to the selected calendar file.
* Calls editNewToDo.
*/
function newToDo ( startDate, dueDate ) 
{
    var calendarToDo = createToDo();
   
    // created todo has no start or due date unless user wants one
    if (startDate) 
        calendarToDo.entryDate = jsDateToDateTime(startDate);

    if (dueDate)
        calendarToDo.dueDate = jsDateToDateTime(startDate);

    setDefaultAlarmValues(calendarToDo);

    var calendar = getSelectedCalendarOrNull();
    
    createTodoWithDialog(calendar, null, null, calendarToDo);
}

/**
 * Get the default calendar selected in the calendars tab.
 * Returns a calICalendar object, or null if none selected.
 */
function getSelectedCalendarOrNull()
{
   var selectedCalendarItem = document.getElementById( "list-calendars-listbox" ).selectedItem;
   
   if ( selectedCalendarItem )
     return selectedCalendarItem.calendar;
   else
     return null;
}

/**
*  This is called from the unifinder's edit command
*/

function editEvent()
{
   if( gCalendarWindow.EventSelection.selectedEvents.length == 1 )
   {
      var calendarEvent = gCalendarWindow.EventSelection.selectedEvents[0];

      if( calendarEvent != null )
      {
         modifyEventWithDialog(getOccurrenceOrParent(calendarEvent));
      }
   }
}

function editToDo(task) {
    if (!task)
        return;

    modifyEventWithDialog(getOccurrenceOrParent(task));
}

/**
*  This is called from the unifinder's delete command
*
*/
function deleteItems( SelectedItems, DoNotConfirm )
{
    if (!SelectedItems)
        return;

    startBatchTransaction();
    for (i in SelectedItems) {
        var aOccurrence = SelectedItems[i];
        if (aOccurrence.parentItem != aOccurrence) {
            var event = aOccurrence.parentItem.clone();
            event.recurrenceInfo.removeOccurrenceAt(aOccurrence.recurrenceId);
            doTransaction('modify', event, event.calendar, aOccurrence.parentItem, null);
        } else {
            doTransaction('delete', aOccurrence, aOccurrence.calendar, null, null);
        }
    }
    endBatchTransaction();
}


/**
*  Delete the current selected item with focus from the ToDo unifinder list
*/
function deleteEventCommand( DoNotConfirm )
{
   var SelectedItems = gCalendarWindow.EventSelection.selectedEvents;
   deleteItems( SelectedItems, DoNotConfirm );
   gCalendarWindow.EventSelection.emptySelection();
}


/**
*  Delete the current selected item with focus from the ToDo unifinder list
*/
function deleteToDoCommand( DoNotConfirm )
{
   var SelectedItems = new Array();
   var tree = document.getElementById( ToDoUnifinderTreeName );
   var start = new Object();
   var end = new Object();
   var numRanges = tree.view.selection.getRangeCount();
   var t;
   var v;
   var toDoItem;
   for (t = 0; t < numRanges; t++) {
      tree.view.selection.getRangeAt(t, start, end);
      for (v = start.value; v <= end.value; v++) {
         toDoItem = tree.taskView.getCalendarTaskAtRow( v );
         SelectedItems.push( toDoItem );
      }
   }
   deleteItems( SelectedItems, DoNotConfirm );
   tree.view.selection.clearSelection();
}


function goFindNewCalendars()
{
   //launch the browser to http://www.apple.com/ical/library/
   var browserService = penapplication.getService( "org.penzilla.browser" );
   if(browserService)
   {
       browserService.setUrl("http://www.icalshare.com/");
       browserService.focusBrowser();
   }
}

function selectAllEvents()
{
    //XXX
    throw "Broken by the switch to the new views"; 
}

function closeCalendar()
{
   self.close();
}

function print()
{
    window.openDialog("chrome://calendar/content/printDialog.xul",
                      "printdialog","chrome");
}

/* Change the only-workday checkbox */
function changeOnlyWorkdayCheckbox() {
    var oldValue = (document.getElementById("toggle_workdays_only")
                           .getAttribute("checked") == 'true');
    document.getElementById("toggle_workdays_only")
            .setAttribute("checked", !oldValue);

    // This does NOT make the views refresh themselves
    for each (view in document.getElementById("view-deck").childNodes) {
        view.workdaysOnly = !oldValue;
    }

    // No point in updating views that aren't shown.  They'll notice the change
    // next time we call their goToDay function, just update the current view
    var currentView = document.getElementById("view-deck").selectedPanel;
    currentView.goToDay(currentView.selectedDay);
}

/* Change the display-todo-inview checkbox */
function changeDisplayToDoInViewCheckbox() {
    var oldValue = (document.getElementById("toggle_tasks_in_view")
                           .getAttribute("checked") == 'true');
    document.getElementById("toggle_tasks_in_view")
            .setAttribute("checked", !oldValue);

    // This does NOT make the views refresh themselves
    for each (view in document.getElementById("view-deck").childNodes) {
        view.tasksInView = !oldValue;
    }

    // No point in updating views that aren't shown.  They'll notice the change
    // next time we call their goToDay function, just update the current view
    var currentView = document.getElementById("view-deck").selectedPanel;
    currentView.goToDay(currentView.selectedDay);
}

// about Calendar dialog
function displayCalendarVersion()
{
  // uses iframe, but iframe does not auto-size per bug 80713, so provide height
  window.openDialog("chrome://calendar/content/about.xul", "About","modal,centerscreen,chrome,width=500,height=450,resizable=yes");
}


// about Sunbird dialog
function openAboutDialog()
{
  window.openDialog("chrome://calendar/content/aboutDialog.xul", "About", "modal,centerscreen,chrome,resizable=no");
}

function openPreferences() {
    // Check to see if the prefwindow is already open
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);

    var win = wm.getMostRecentWindow("Calendar:Preferences");
    var url = "chrome://calendar/content/preferences/preferences.xul";
    var features = "chrome,titlebar,toolbar,centerscreen,dialog=no";

    if (win) {
        win.focus();
    } else {
        openDialog(url, "Preferences", features);
    }
}

/**
 * Opens the release notes page for this version of the application.
 */
function openReleaseNotes() {
    var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                            .getService(Components.interfaces.nsIXULAppInfo);
    var calendarBundle = document.getElementById("bundle_branding");
    var relNotesURL = calendarBundle.getFormattedString("releaseNotesURL",
                                                        [appInfo.version]);
    launchBrowser(relNotesURL);
}

// Next two functions make the password manager menu option
// only show up if there is a wallet component. Assume that
// the existence of a wallet component means wallet UI is there too.
function checkWallet()
{
  if ('@mozilla.org/wallet/wallet-service;1' in Components.classes) {
    document.getElementById("password-manager-menu")
            .removeAttribute("hidden");
  }
}

function openWalletPasswordDialog()
{
  window.openDialog("chrome://communicator/content/wallet/SignonViewer.xul",
                    "_blank","chrome,resizable=yes","S");
}

var strBundleService = null;
function srGetStrBundle(path)
{
  var strBundle = null;

  if (!strBundleService) {
      try {
          strBundleService =
              Components.classes["@mozilla.org/intl/stringbundle;1"].getService();
          strBundleService =
              strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
      } catch (ex) {
          dump("\n--** strBundleService failed: " + ex + "\n");
          return null;
      }
  }

  strBundle = strBundleService.createBundle(path);
  if (!strBundle) {
        dump("\n--** strBundle createInstance failed **--\n");
  }
  return strBundle;
}

function CalendarCustomizeToolbar()
{
  // Disable the toolbar context menu items
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", true);
    
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.setAttribute("disabled", "true");
  
  window.openDialog("chrome://calendar/content/customizeToolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById("calendar-toolbox"));
}

function CalendarToolboxCustomizeDone(aToolboxChanged)
{
  // Re-enable parts of the UI we disabled during the dialog
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", false);
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.removeAttribute("disabled");

  // XXX Shouldn't have to do this, but I do
  window.focus();
}

function updateUndoRedoMenu() {
    if (gTransactionMgr.numberOfUndoItems)
        document.getElementById('undo_command').removeAttribute('disabled');
    else    
        document.getElementById('undo_command').setAttribute('disabled', true);

    if (gTransactionMgr.numberOfRedoItems)
        document.getElementById('redo_command').removeAttribute('disabled');
    else    
        document.getElementById('redo_command').setAttribute('disabled', true);
}

function openLocalCalendar() {

    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, gCalendarBundle.getString("Open"), nsIFilePicker.modeOpen);
    fp.appendFilter(gCalendarBundle.getString("filterCalendar"), "*.ics");
    fp.appendFilters(nsIFilePicker.filterAll);
 
    if (fp.show() != nsIFilePicker.returnOK) {
        return;
    }	
    
    var url = fp.fileURL.spec;
    var calMgr = getCalendarManager();
    var composite = getDisplayComposite();
    var openCalendar = calMgr.createCalendar("ics", makeURL(url));
    calMgr.registerCalendar(openCalendar);
     
    // Strip ".ics" from filename for use as calendar name, taken from calendarCreation.js
    var fullPathRegex = new RegExp("([^/:]+)[.]ics$");
    var prettyName = url.match(fullPathRegex);
    var name;
        
    if (prettyName && prettyName.length >= 1) {
        name = decodeURIComponent(prettyName[1]);
    } else {
        name = gCalendarBundle.getString("untitledCalendarName");
    }
        
    openCalendar.name = name;
}

/** Sets the selected day in the minimonth to the currently selected day
    in the embedded view.
 */
function observeViewDaySelect(event) {

    var date = event.detail;
    var jsDate = new Date(date.year, date.month, date.day);

    // for the month and multiweek view find the main month,
    // which is the month with the most visible days in the view;
    // note, that the main date is the first day of the main month
    var jsMainDate;
    if (!event.originalTarget.supportsDisjointDates) {
        var mainDate = null;
        var maxVisibleDays = 0;
        var currentView = document.getElementById("view-deck").selectedPanel;
        var startDay = currentView.startDay;
        var endDay = currentView.endDay;
        var firstMonth = startDay.startOfMonth;
        var lastMonth = endDay.startOfMonth;
        for (var month = firstMonth.clone(); month.compare(lastMonth) <= 0; month.month += 1, month.normalize()) {
            var visibleDays = 0;
            if (month.compare(firstMonth) == 0) {
                visibleDays = startDay.endOfMonth.day - startDay.day + 1;
            } else if (month.compare(lastMonth) == 0) {
                visibleDays = endDay.day;
            } else {
                visibleDays = month.endOfMonth.day;
            }
            if (visibleDays > maxVisibleDays) {
                mainDate = month.clone();
                maxVisibleDays = visibleDays;
            }
        }
        jsMainDate = new Date(mainDate.year, mainDate.month, mainDate.day);
    }

    document.getElementById("lefthandcalendar").selectDate(jsDate, jsMainDate);
}
