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
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Robin Edrenius <robin.edrenius@gmail.com>
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

/*-----------------------------------------------------------------
*  C A L E N D A R     C L A S S E S
*/

function calViewController() {}

calViewController.prototype.QueryInterface = function(aIID) {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.calICalendarViewController)) {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
}

calViewController.prototype.createNewEvent = function (aCalendar, aStartTime, aEndTime) {
    if (aStartTime && aEndTime && !aStartTime.isDate && !aEndTime.isDate) {
        var event = createEvent();
        event.startDate = aStartTime;
        event.endDate = aEndTime;
        var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                            .getService(Components.interfaces.nsIStringBundleService);
        var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
        event.title = props.GetStringFromName("newEvent");
        setDefaultAlarmValues(event);
        doTransaction('add', event, aCalendar, null, null);
    } else if (aStartTime && aStartTime.isDate) {
        var event = createEvent();
        event.startDate = aStartTime;
        setDefaultAlarmValues(event);
        doTransaction('add', event, aCalendar, null, null);
    } else {
        newEvent();
    }
}

calViewController.prototype.modifyOccurrence = function (aOccurrence, aNewStartTime, aNewEndTime) {
    if (aNewStartTime && aNewEndTime && !aNewStartTime.isDate 
        && !aNewEndTime.isDate) {
        var itemToEdit = getOccurrenceOrParent(aOccurrence);
        if (!itemToEdit) {
            return;
        }
        var instance = itemToEdit.clone();
        
        var newStartTime = aNewStartTime;
        var newEndTime = aNewEndTime;

        // if we're about to modify the parentItem, we need to account
        // for the possibility that the item passed as argument was
        // some other occurrence, but the user said she would like to
        // modify all occurrences instead.
        if (instance.parentItem == instance) {
            var instanceStart = instance.startDate || instance.entryDate;
            var occurrenceStart = aOccurrence.startDate || aOccurrence.entryDate;
            var startDiff = instanceStart.subtractDate(occurrenceStart);
            aNewStartTime.addDuration(startDiff);
            var instanceEnd = instance.endDate || instance.dueDate;
            var occurrenceEnd = aOccurrence.endDate || aOccurrence.dueDate;
            var endDiff = instanceEnd.subtractDate(occurrenceEnd);
            aNewEndTime.addDuration(endDiff);
        }

        if (instance instanceof Components.interfaces.calIEvent) {
            instance.startDate = aNewStartTime;
            instance.endDate = aNewEndTime;
        } else {
            instance.entryDate = aNewStartTime;
            instance.dueDate = aNewEndTime;
        }
        doTransaction('modify', instance, instance.calendar, itemToEdit, null);
    } else {
        editEvent();
    }
}

calViewController.prototype.deleteOccurrence = function (aOccurrence) {
    var itemToDelete = getOccurrenceOrParent(aOccurrence);
    if (!itemToDelete) {
        return;
    }
    if (itemToDelete.parentItem != itemToDelete) {
        var event = itemToDelete.parentItem.clone();
        event.recurrenceInfo.removeOccurrenceAt(itemToDelete.recurrenceId);
        doTransaction('modify', event, event.calendar, itemToDelete.parentItem, null);
    } else {
        doTransaction('delete', itemToDelete, itemToDelete.calendar, null, null);
    }
}

var gViewController = new calViewController();

/*-----------------------------------------------------------------
*   CalendarWindow Class
*
*  Maintains the three calendar views and the selection.
*
* PROPERTIES
*     selectedDate           - selected date, instance of Date 
*  
*/


/**
*   CalendarWindow Constructor.
*
* NOTES
*   There is one instance of CalendarWindow
*   WARNING: As much as we all love CalendarWindow, it's going to die soon.
*/

function CalendarWindow( )
{
   //setup the calendar event selection
   this.EventSelection = new CalendarEventSelection( this );
   gViewController.selectionManager = this.EventSelection;

   /** This object only exists to keep too many things from breaking during the
    *   switch to the new views
   **/
   this.currentView = {
       hiliteTodaysDate: function() {
           document.getElementById("view-deck").selectedPanel.goToDay(now());
       },
       // This will get converted again to a calDateTime, which is silly, but let's
       // not change too much right now.
       getNewEventDate: function() {
           var d = document.getElementById("view-deck").selectedPanel.selectedDay;
           if (!d)
               d = now();
           d = d.getInTimezone("floating");
           d.hour = now().hour;
           return d.jsDate;
       },
       get eventList() { return new Array(); },
       goToNext: function() {
           document.getElementById("view-deck").selectedPanel.moveView(1);
       },
       goToPrevious: function() {
           document.getElementById("view-deck").selectedPanel.moveView(-1);
       },
       changeNumberOfWeeks: function(menuitem) {
           var mwView = document.getElementById("view-deck").selectedPanel;
           mwView.weeksInView = menuitem.value;
       },
       get selectedDate() { return this.getNewEventDate(); },
       get displayStartDate() { return document.getElementById("view-deck").selectedPanel.startDay; },
       get displayEndDate() { return document.getElementById("view-deck").selectedPanel.endDay; },
       getVisibleEvent: function(event) { 
           var view = document.getElementById("view-deck").selectedPanel
           if ((event.startDate.compare(view.endDay) != 1) &&
               (event.endDate.compare(view.startDay) != -1)) {
               return true;
           }
           return false;
       },
       goToDay: function(newDate) {
           document.getElementById("view-deck").selectedPanel.goToDay(jsDateToDateTime(newDate));
       },
       refresh: function() {
           this.goToDay(this.getNewEventDate());
       }
   };

   // Get the last view that was shown before shutdown, and switch to it
   var SelectedIndex = document.getElementById("view-deck").selectedIndex;
   
   switch( SelectedIndex )
   {
      case "1":
         this.switchToWeekView();
         break;
      case "2":
         this.switchToMultiweekView();
         break;
      case "3":
         this.switchToMonthView();
         break;
      case "0":
      default:
         this.switchToDayView();
         break;
   }
}

/** PUBLIC
*
*   Switch to the day view if it isn't already the current view
*/

CalendarWindow.prototype.switchToDayView = function calWin_switchToDayView( )
{
    document.getElementById("month_view_command").removeAttribute("checked");
    document.getElementById("multiweek_view_command").removeAttribute("checked");
    document.getElementById("week_view_command").removeAttribute("checked");
    document.getElementById("day_view_command").setAttribute("checked", true);
    document.getElementById("menu-numberofweeks-inview").setAttribute("disabled", true);
    this.switchToView('day-view');
}


/** PUBLIC
*
*   Switch to the week view if it isn't already the current view
*/

CalendarWindow.prototype.switchToWeekView = function calWin_switchToWeekView( )
{
    document.getElementById("month_view_command").removeAttribute("checked");
    document.getElementById("multiweek_view_command").removeAttribute("checked");
    document.getElementById("day_view_command").removeAttribute("checked");
    document.getElementById("week_view_command").setAttribute("checked", true);
    document.getElementById("menu-numberofweeks-inview").setAttribute("disabled", true);
    this.switchToView('week-view');
}


/** PUBLIC
*
*   Switch to the month view if it isn't already the current view
*/

CalendarWindow.prototype.switchToMonthView = function calWin_switchToMonthView( )
{
    document.getElementById("week_view_command").removeAttribute("checked");
    document.getElementById("multiweek_view_command").removeAttribute("checked");
    document.getElementById("day_view_command").removeAttribute("checked");
    document.getElementById("month_view_command").setAttribute("checked", true);
    document.getElementById("menu-numberofweeks-inview").setAttribute("disabled", true);
    this.switchToView('month-view');
}

/** PUBLIC
*
*   Switch to the multiweek view if it isn't already the current view
*/

CalendarWindow.prototype.switchToMultiweekView = function calWin_switchToMultiweekView( )
{
    document.getElementById("month_view_command").removeAttribute("checked");
    document.getElementById("week_view_command").removeAttribute("checked");
    document.getElementById("day_view_command").removeAttribute("checked");
    document.getElementById("multiweek_view_command").setAttribute("checked", true);
    document.getElementById("menu-numberofweeks-inview").removeAttribute("disabled");
    this.switchToView('multiweek-view');
}

/** PUBLIC
*
*   Display today in the current view
*/

CalendarWindow.prototype.goToToday = function calWin_goToToday( )
{
   document.getElementById("lefthandcalendar").value = new Date();
   document.getElementById("view-deck").selectedPanel.goToDay(now());
}
/** PUBLIC
*
*   Choose a date, then go to that date in the current view.
*/

CalendarWindow.prototype.pickAndGoToDate = function calWin_pickAndGoToDate( )
{
  var currentView = document.getElementById("view-deck").selectedPanel;
  var args = new Object();
  args.initialDate = this.getSelectedDate();  
  args.onOk = function receiveAndGoToDate( pickedDate ) {
    currentView.goToDay( jsDateToDateTime(pickedDate) );
    document.getElementById( "lefthandcalendar" ).value = pickedDate;
  };
  openDialog("chrome://calendar/content/goToDateDialog.xul",
             "GoToDateDialog", // target= window name
             "chrome,modal", args);
}

/** PUBLIC
*
*   Go to the next period in the current view
*/

CalendarWindow.prototype.goToNext = function calWin_goToNext( value )
{
    document.getElementById("view-deck").selectedPanel.moveView(value);
}


/** PUBLIC
*
*   Go to the previous period in the current view
*/

CalendarWindow.prototype.goToPrevious = function calWin_goToPrevious( value )
{   
    document.getElementById("view-deck").selectedPanel.moveView(-1*value);
}


/** PUBLIC
*
*   Go to today in the current view
*/

CalendarWindow.prototype.goToDay = function calWin_goToDay( newDate )
{
    var view = document.getElementById("view-deck").selectedPanel;
    var cdt = Components.classes["@mozilla.org/calendar/datetime;1"]
                        .createInstance(Components.interfaces.calIDateTime);
    cdt.year = newDate.getFullYear();
    cdt.month = newDate.getMonth();
    cdt.day = newDate.getDate();
    cdt.isDate = true;
    cdt.timezone = view.timezone;
    view.goToDay(cdt);
}

/** PUBLIC
*
*   Get the selected date
*/

CalendarWindow.prototype.getSelectedDate = function calWin_getSelectedDate( )
{
    // unifinder.js wants this as a js-date, stupid timezone issues between
    // js-dates and calDateTimes
    return document.getElementById("view-deck").selectedPanel.selectedDay
                   .getInTimezone("floating").jsDate;
}


/** PRIVATE
*
*   Helper function to switch to a view
*
* PARAMETERS
*   newView  - MUST be one of the three CalendarView instances created in the constructor
*/

CalendarWindow.prototype.switchToView = function calWin_switchToView( newView )
{
    var viewElement = document.getElementById(newView);

    // If this is the first time we've shown the view, (or if someone changed
    // our composite), then we need to set all this stuff.
    if (viewElement.displayCalendar != getDisplayComposite()) {
        viewElement.controller = gViewController;
        viewElement.displayCalendar = getDisplayComposite();
        viewElement.timezone = calendarDefaultTimezone();
        this.EventSelection.addObserver(viewElement.selectionObserver);
    }

    var deck = document.getElementById("view-deck");
    var day;
    try {
        day = deck.selectedPanel.selectedDay;
    } catch(ex) {} // Fails if no view has ever been shown this session

    // Should only happen on first startup
    if (!day)
        day = now();
    deck.selectedPanel = viewElement;
    deck.selectedPanel.goToDay(day);

    var prevCommand = document.getElementById("calendar-go-menu-previous");
    prevCommand.setAttribute("label", prevCommand.getAttribute("label-"+newView));
    var nextCommand = document.getElementById("calendar-go-menu-next");
    nextCommand.setAttribute("label", nextCommand.getAttribute("label-"+newView));
}

CalendarWindow.prototype.onMouseUpCalendarSplitter = function calWinOnMouseUpCalendarSplitter()
{
    return;
}

CalendarWindow.prototype.onMouseUpCalendarViewSplitter = function calWinOnMouseUpCalendarViewSplitter()
{
   //check if calendar-view-splitter is collapsed
   if( document.getElementById( "bottom-events-box" ).getAttribute( "collapsed" ) != "true" )
   {
      //do this because if they started with it collapsed, its not showing anything right now.

      //in a setTimeout to give the pull down menu time to draw.
      setTimeout( "refreshEventTree();", 10 );
   }
}

/** PUBLIC
*   The resize handler, used to set the size of the views so they fit the screen.
*/
CalendarWindow.prototype.doResize = function calWin_doResize(){
  if( gCalendarWindow )
    return;
}
