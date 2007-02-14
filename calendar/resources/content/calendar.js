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
 *                 Matthew Willis <lilmatt@mozilla.com>
 *                 Joey Minta <jminta@gmail.com>
 *                 Dan Mosedale <dan.mosedale@oracle.com>
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

// store the current selection in the global scope to workaround bug 351747
var gXXXEvilHackSavedSelection;

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

   // tasksInView is false by default
   if (document.getElementById("toggle_tasks_in_view").getAttribute("checked")
       == 'true') {
       var deck = document.getElementById("view-deck")
       for each (view in deck.childNodes) {
           view.tasksInView = true;
       }
       deck.selectedPanel.goToDay(deck.selectedPanel.selectedDay);
   }

   // set up the unifinder
   
   prepareCalendarUnifinder();

   prepareCalendarToDoUnifinder();
   
   scheduleMidnightUpdate(refreshUIBits);

   initCalendarManager();

   // fire up the alarm service
   var alarmSvc = Components.classes["@mozilla.org/calendar/alarm-service;1"]
                  .getService(Components.interfaces.calIAlarmService);
   alarmSvc.timezone = calendarDefaultTimezone();
   alarmSvc.startup();

   // a bit of a hack since the menulist doesn't remember the selected value
   var value = document.getElementById( 'event-filter-menulist' ).value;
   document.getElementById( 'event-filter-menulist' ).selectedItem = 
       document.getElementById( 'event-filter-'+value );

   var toolbox = document.getElementById("calendar-toolbox");
   toolbox.customizeDone = CalendarToolboxCustomizeDone;

   getViewDeck().addEventListener("dayselect", observeViewDaySelect, false);
   getViewDeck().addEventListener("itemselect", onSelectionChanged, true);

   // Handle commandline args
   for (var i=0; i < window.arguments.length; i++) {
       try {
           var cl = window.arguments[i].QueryInterface(Components.interfaces.nsICommandLine);
       } catch(ex) {
           dump("unknown argument passed to main window\n");
           continue;
       }
       handleCommandLine(cl);
   }
}

function handleCommandLine(aComLine) {
    var calurl;
    try {
        calurl = aComLine.handleFlagWithParam("subscribe", false);
    } catch(ex) {}
    if (calurl) {
        var uri = aComLine.resolveURI(calurl);
        var cal = getCalendarManager().createCalendar('ics', uri);
        getCalendarManager().registerCalendar(cal);

        // Strip ".ics" from filename for use as calendar name, taken from 
        // calendarCreation.js
        var fullPathRegex = new RegExp("([^/:]+)[.]ics$");
        var prettyName = calurl.match(fullPathRegex);
        var name;

        if (prettyName && prettyName.length >= 1) {
            name = decodeURIComponent(prettyName[1]);
        } else {
            name = calGetString("calendar", "untitledCalendarName");
        }
        cal.name = name;
    }

    var date;
    try {
        date = aComLine.handleFlagWithParam("showdate", false);
    } catch(ex) {}
    if (date) {
        currentView().goToDay(jsDateToDateTime(new Date(date)));
    }
}

/* Called at midnight to tell us to update the views and other ui bits */
function refreshUIBits() {
    currentView().goToDay(now());
    refreshEventTree();

    // and schedule again...
    scheduleMidnightUpdate(refreshUIBits);
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
        getCompositeCalendar().removeCalendar(calendar.uri);
        var calMgr = getCalendarManager();
        calMgr.unregisterCalendar(calendar);
        calMgr.deleteCalendar(calendar);
    }
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
    var items = [];
    var listener = {
        onOperationComplete: function selectAll_ooc(aCalendar, aStatus, 
                                                    aOperationType, aId, 
                                                    aDetail) {
            currentView().setSelectedItems(items.length, items, false);
        },
        onGetResult: function selectAll_ogr(aCalendar, aStatus, aItemType, 
                                            aDetail, aCount, aItems) {
            for each (var item in aItems) {
                items.push(item);
            }
        }
    };

    var composite = getCompositeCalendar();
    var filter = composite.ITEM_FILTER_COMPLETED_ALL |
                 composite.ITEM_FILTER_CLASS_OCCURRENCES;

    if (currentView().tasksInView) {
        filter |= composite.ITEM_FILTER_TYPE_ALL; 
    } else {
        filter |= composite.ITEM_FILTER_TYPE_EVENT;
    }

    // Need to move one day out to get all events
    var end = currentView().endDay.clone();
    end.day += 1;
    end.normalize();

    composite.getItems(filter, 0, currentView().startDay, end, listener);
}

function closeCalendar()
{
   self.close();
}

function onSelectionChanged(aEvent) {
    var elements = 
        document.getElementsByAttribute("disabledwhennoeventsselected", "true");

    var selectedItems = aEvent.detail;
    gXXXEvilHackSavedSelection = selectedItems;

    for (var i = 0; i < elements.length; i++) {
        if (selectedItems.length >= 1) {
            elements[i].removeAttribute("disabled");
        } else {
            elements[i].setAttribute("disabled", "true");
        }
    }
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

function openPreferences() {
    // Check to see if the prefwindow is already open
    var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);

    var win = wm.getMostRecentWindow("Calendar:Preferences");

    if (win) {
        win.focus();
    } else {
        // The prefwindow should only be modal on non-instant-apply platforms
        var instApply = getPrefSafe("browser.preferences.instantApply", false);

        var features = "chrome,titlebar,toolbar,centerscreen," +
                       (instApply ? "dialog=no" : "modal");

        var url = "chrome://calendar/content/preferences/preferences.xul";

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

function CalendarCustomizeToolbar()
{
  // Disable the toolbar context menu items
  var menubar = document.getElementById("main-menubar");
  for (var i = 0; i < menubar.childNodes.length; ++i)
    menubar.childNodes[i].setAttribute("disabled", true);
    
  var cmd = document.getElementById("cmd_CustomizeToolbars");
  cmd.setAttribute("disabled", "true");

#ifdef MOZILLA_1_8_BRANCH
  window.openDialog("chrome://calendar/content/customizeToolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById("calendar-toolbox"));
#else
  window.openDialog("chrome://global/content/customizeToolbar.xul", "CustomizeToolbar",
                    "chrome,all,dependent", document.getElementById("calendar-toolbox"));
#endif
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
    var wildmat = "*.ics";
    var description = gCalendarBundle.getFormattedString("filterIcs", [wildmat]);
    fp.appendFilter(description, wildmat);
    fp.appendFilters(nsIFilePicker.filterAll);
 
    if (fp.show() != nsIFilePicker.returnOK) {
        return;
    }	
    
    var url = fp.fileURL.spec;
    var calMgr = getCalendarManager();
    var composite = getCompositeCalendar();
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
