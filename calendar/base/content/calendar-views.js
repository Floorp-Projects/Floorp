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
 * The Original Code is Calendar views code
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Calendar Squad
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Joey Minta <jminta@gmail.com>
 *   Michael Buettner <michael.buettner@sun.com>
 *   gekacheka@yahoo.com
 *   Matthew Willis <lilmatt@mozilla.com>
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

var calendarViewController = {
    QueryInterface: function(aIID) {
        if (!aIID.equals(Components.interfaces.calICalendarViewController) &&
            !aIID.equals(Components.interfaces.nsISupports)) {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    createNewEvent: function (aCalendar, aStartTime, aEndTime) {
        // XXX If we're adding an item from the view, let's make sure that
        // XXX the calendar in question is visible!
        // XXX unify these
        if (!aCalendar) {
            if ("ltnSelectedCalendar" in window) {
                aCalendar = ltnSelectedCalendar();
            } else {
                aCalendar = getSelectedCalendarOrNull();
            }
        }

        // if we're given both times, skip the dialog
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
            createEventWithDialog(aCalendar, aStartTime, null);
        }
    },

    pendingJobs: [],

    // in order to initiate a modification for the occurrence passed as argument
    // we create an object that records the necessary details and store it in an
    // internal array ('pendingJobs'). this way we're in a position to terminate
    // any pending modification if need should be.
    createPendingModification: function (aOccurrence) {
        // finalize a (possibly) pending modification. this will notify
        // an open dialog to save any outstanding modifications.
        aOccurrence = this.finalizePendingModification(aOccurrence);

        var pendingModification = {
            controller: this,
            item: aOccurrence,
            finalize: null,
            dispose: function() {
                var array = this.controller.pendingJobs;
                for (var i=0; i<array.length; i++) {
                    if (array[i] == this) {
                        array.splice(i,1);
                        break;
                    }
                }
            }
        }

        this.pendingJobs.push(pendingModification);

        modifyEventWithDialog(aOccurrence,pendingModification);
    },

    // iterate the list of pending modifications and see if the occurrence
    // passed as argument is currently about to be modified (event dialog is
    // open with the item in question). if this should be the case we call
    // finalize() in order to bring the dialog down and avoid dataloss.
    finalizePendingModification: function (aOccurrence) {

      for each (var job in this.pendingJobs) {
          var item = job.item;
          var parent = item.parent;
          if (item.hasSameIds(aOccurrence) ||
              item.parentItem.hasSameIds(aOccurrence) ||
              item.hasSameIds(aOccurrence.parentItem)) {
              // terminate() will most probably create a modified item instance.
              aOccurrence = job.finalize();
              break;
        }
      }

      return aOccurrence;
    },

    modifyOccurrence: function (aOccurrence, aNewStartTime, aNewEndTime, aNewTitle) {

        aOccurrence = this.finalizePendingModification(aOccurrence);

        // if modifying this item directly (e.g. just dragged to new time),
        // then do so; otherwise pop up the dialog
        if ((aNewStartTime && aNewEndTime) || aNewTitle) {
            var instance = aOccurrence.clone();

            if (aNewTitle) {
                instance.title = aNewTitle;
            }

            // When we made the executive decision (in bug 352862) that
            // dragging an occurrence of a recurring event would _only_ act
            // upon _that_ occurrence, we removed a bunch of code from this
            // function. If we ever revert that decision, check CVS history
            // here to get that code back.

            if (aNewStartTime) { // we know we have aEndTime too then
                // Yay for variable names that make this next line look silly
                if (instance instanceof Components.interfaces.calIEvent) {
                    instance.startDate = aNewStartTime;
                    instance.endDate = aNewEndTime;
                } else {
                    instance.entryDate = aNewStartTime;
                    instance.dueDate = aNewEndTime;
                }
            }
            doTransaction('modify', instance, instance.calendar, aOccurrence, null);
        } else {
            // prompt for choice between occurrence and master for recurrent items
            var itemToEdit = getOccurrenceOrParent(aOccurrence);
            if (!itemToEdit) {
                return;  // user cancelled
            }

            this.createPendingModification(itemToEdit);
        }
    },

    deleteOccurrence: function (aOccurrence) {
        var itemToDelete = getOccurrenceOrParent(aOccurrence);
        if (!itemToDelete) {
            return;
        }
        itemToDelete = this.finalizePendingModification(itemToDelete);
        if (!itemToDelete.parentItem.hasSameIds(itemToDelete)) {
            var event = itemToDelete.parentItem.clone();
            event.recurrenceInfo.removeOccurrenceAt(itemToDelete.recurrenceId);
            doTransaction('modify', event, event.calendar, itemToDelete.parentItem, null);
        } else {
            doTransaction('delete', itemToDelete, itemToDelete.calendar, null, null);
        }
    }
};

function switchToView(aViewType) {
    var viewDeck = getViewDeck();
    var selectedDay;
    var currentSelection = [];
    try {
        selectedDay = viewDeck.selectedPanel.selectedDay;
        currentSelection = viewDeck.selectedPanel.getSelectedItems({});
    } catch(ex) {
        // This dies if no view has even been chosen this session, but that's
        // ok because we'll just use now() below.
    } 

    if (!selectedDay)
        selectedDay = now();

    // Anyone wanting to plug in a view needs to follow this naming scheme
    var view = document.getElementById(aViewType+"-view");
    viewDeck.selectedPanel = view;

    var compositeCal = getCompositeCalendar();
    if (view.displayCalendar != compositeCal) {
        view.displayCalendar = compositeCal;
        view.timezone = calendarDefaultTimezone();
        view.controller = calendarViewController;
    }

    view.goToDay(selectedDay);
    view.setSelectedItems(currentSelection.length, currentSelection);
}

function moveView(aNumber) {
    currentView().moveView(aNumber);
}

// Helper function to get the view deck in a neutral way, regardless of whether
// we're in Sunbird or Lightning.
function getViewDeck() {
    var sbDeck = document.getElementById("view-deck");
    var ltnDeck = document.getElementById("calendar-view-box");
    return sbDeck || ltnDeck;
}

/**
 * Returns the currently visible calendar view.
 */
function currentView() {
    return getViewDeck().selectedPanel;
}

/**
 * Returns the selected day in the views in a app (Sunbird vs. Lightning)
 * neutral way
 */
function getSelectedDay() {
    return currentView().selectedDay;
}

/** Creates a timer that will fire after midnight.  Pass in a function as 
 * aRefreshCallback that should be called at that time.
 */
function scheduleMidnightUpdate(aRefreshCallback) {
    var jsNow = new Date();
    var tomorrow = new Date(jsNow.getFullYear(), jsNow.getMonth(), jsNow.getDate() + 1);
    var msUntilTomorrow = tomorrow.getTime() - jsNow.getTime();

    // Is an nsITimer/callback extreme overkill here? Yes, but it's necessary to
    // workaround bug 291386.  If we don't, we stand a decent chance of getting
    // stuck in an infinite loop.
    var udCallback = {
        notify: function(timer) {
            aRefreshCallback();
        }
    };

    var timer = Components.classes["@mozilla.org/timer;1"]
                          .createInstance(Components.interfaces.nsITimer);
    timer.initWithCallback(udCallback, msUntilTomorrow, timer.TYPE_ONE_SHOT);
}

// Returns the actual style sheet object with the specified path.  Callers are
// responsible for any caching they may want to do.
function getStyleSheet(aStyleSheetPath) {
    for each (var sheet in document.styleSheets) {
        if (sheet.href == aStyleSheetPath) {
            return sheet;
        }
    }
}

// Updates the style rules for a particular object.  If the object is a
// category (and hence doesn't have a uri), we set the border color.  If
// it's a calendar, we set the background color
function updateStyleSheetForObject(aObject, aSheet) {
    var selectorPrefix, name, ruleUpdaterFunc;
    if (aObject.uri) {
        // This is a calendar, so we're going to set the background color
        name = aObject.uri.spec;
        selectorPrefix = "item-calendar=";
        ruleUpdaterFunc = function calendarRuleFunc(aRule, aIndex) {
            var color = getCalendarManager().getCalendarPref(aObject, 'color');
            if (!color) {
                color = "#A8C2E1";
            }
            aRule.style.backgroundColor = color;
            aRule.style.color = getContrastingTextColor(color);
        };
    } else {
        // This is a category, where we set the border.  Also note that we 
        // use the ~= selector, since there could be multiple categories
        name = aObject.replace(' ','_');
        selectorPrefix = "item-category~=";
        ruleUpdaterFunc = function categoryRuleFunc(aRule, aIndex) {
            var color = getPrefSafe("calendar.category.color."+aObject, null);
            if (color) {
                aRule.style.border = color + " solid 2px";
            } else {
                aSheet.deleteRule(aIndex);
            }
        };
    }

    var selector = '.calendar-item[' + selectorPrefix + '"' + name + '"]';

    // Now go find our rule
    var rule, ruleIndex;
    for (var i = 0; i < aSheet.cssRules.length; i++) {
        var maybeRule = aSheet.cssRules[i];
        if (maybeRule.selectorText && (maybeRule.selectorText == selector)) {
            rule = maybeRule;
            ruleIndex = i;
            break;
        }
    }

    if (!rule) {
        aSheet.insertRule(selector + ' { }', aSheet.cssRules.length);
        rule = aSheet.cssRules[aSheet.cssRules.length-1];
    }

    ruleUpdaterFunc(rule, ruleIndex);
}

/** 
 *  Sets the selected day in the minimonth to the currently selected day
 *  in the embedded view.
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
        var startDay = currentView().startDay;
        var endDay = currentView().endDay;
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

    getMinimonth().selectDate(jsDate, jsMainDate);
    currentView().focus();
}

/** Provides a neutral way to get the minimonth, regardless of whether we're in
 * Sunbird or Lightning.
 */
function getMinimonth() {
    var sbMinimonth = document.getElementById("lefthandcalendar");
    return sbMinimonth || document.getElementById("ltnMinimonth");
}
