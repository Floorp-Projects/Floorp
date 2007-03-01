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
 *                 Michiel van Leeuwen <mvl@exedo.nl>
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

var gPrintSettings = null;

function getCalendarView()
{
    var theView = window.opener.currentView();
    if (!theView.startDay) {
        theView = null;
    }
    return theView;
}

function loadCalendarPrintDialog()
{
    // set the datepickers to the currently selected dates
    var theView = getCalendarView();
    if (theView) {
        document.getElementById("start-date-picker").value = theView.startDay.jsDate;
        document.getElementById("end-date-picker").value = theView.endDay.jsDate;
    } else {
        document.getElementById("printCurrentViewRadio").setAttribute("disabled", true);
    }
    if (!theView || !theView.getSelectedItems({}).length) {
        document.getElementById("selected").setAttribute("disabled", true);
    }
    document.getElementById(theView ? "printCurrentViewRadio" : "custom-range")
            .setAttribute("selected", true);

    // Get a list of formatters
    var contractids = new Array();
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    var catenum = catman.enumerateCategory('cal-print-formatters');

    // Walk the list, adding items to the layout menupopup
    var layoutList = document.getElementById("layout-field");
    while (catenum.hasMoreElements()) {
        var entry = catenum.getNext();
        entry = entry.QueryInterface(Components.interfaces.nsISupportsCString);
        var contractid = catman.getCategoryEntry('cal-print-formatters', entry);
        var formatter = Components.classes[contractid]
                                  .getService(Components.interfaces.calIPrintFormatter);
        // Use the contractid as value
        layoutList.appendItem(formatter.name, contractid);
    }
    layoutList.selectedIndex = 0;

    opener.setCursor("auto");

    refreshHtml();

    self.focus();
}

/**
 * Gets the settings from the dialog's UI widgets.
 * notifies an Object with title, layoutCId, eventList, start, and end
 *          properties containing the appropriate values.
 */
function getEventsAndDialogSettings(receiverFunc)
{
    var settings = new Object();
    var tempTitle = document.getElementById("title-field").value;
    settings.title = (tempTitle || calGetString("calendar", "Untitled"));
    settings.layoutCId = document.getElementById("layout-field").value;
    settings.start = null;
    settings.end = null;
    settings.eventList = null;

    var theView = getCalendarView();
    switch (document.getElementById("view-field").selectedItem.value) {
        case 'currentview':
        case '': //just in case
            settings.start = theView.startDay;
            settings.end   = theView.endDay;
            break;
        case 'selected':
            settings.eventList = theView.getSelectedItems({});
            break;
        case 'custom':
            // We return the time from the timepickers using the selected
            // timezone, as not doing so in timezones with a positive offset
            // from UTC may cause the printout to include the wrong days.
            var currentTimezone = calendarDefaultTimezone();
            settings.start = jsDateToDateTime(document.getElementById("start-date-picker").value);
            settings.start = settings.start.getInTimezone(currentTimezone);
            settings.end   = jsDateToDateTime(document.getElementById("end-date-picker").value);
            settings.end   = settings.end.getInTimezone(currentTimezone);
            break ;
        default :
            dump("Error : no case in printDialog.js::printCalendar()");
    }

    if (settings.eventList) {
        receiverFunc(settings);
    } else {
        // end isn't exclusive, so we need to add one day
        settings.end = settings.end.clone();
        settings.end.day = settings.end.day + 1;
        settings.end.normalize();
        settings.eventList = [];
        var listener = {
            onOperationComplete:
            function onOperationComplete(aCalendar, aStatus, aOperationType, aId, aDateTime) {
                receiverFunc(settings);
            },
            onGetResult:
            function onGetResult(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
                settings.eventList = settings.eventList.concat(aItems);
            }
        };
        window.opener.getCompositeCalendar().getItems(
            Components.interfaces.calICalendar.ITEM_FILTER_TYPE_EVENT | 
            Components.interfaces.calICalendar.ITEM_FILTER_CLASS_OCCURRENCES,
            0, settings.start, settings.end, listener);
    }
}

/**
 * Looks at the selections the user has made (start date, layout, etc.), and
 * updates the HTML in the iframe accordingly. This is also called when a
 * dialog UI element has changed, since we'll want to refresh the preview.
 */
function refreshHtml(finishFunc)
{
    getEventsAndDialogSettings(
        function getEventsAndDialogSettings_response(settings) {
            // calGetString can't do "formatStringFromName".
            var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                .getService(Components.interfaces.nsIStringBundleService);
            var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
            document.title = props.formatStringFromName("PrintPreviewWindowTitle", [settings.title], 1);

            var printformatter = Components.classes[settings.layoutCId]
                                           .createInstance(Components.interfaces.calIPrintFormatter);

            var html = "";
            try {
                var pipe = Components.classes["@mozilla.org/pipe;1"]
                                     .createInstance(Components.interfaces.nsIPipe);
                pipe.init(true, true, 0, 0, null);
                printformatter.formatToHtml(pipe.outputStream,
                                            settings.start,
                                            settings.end,
                                            settings.eventList.length,
                                            settings.eventList,
                                            settings.title);
                pipe.outputStream.close();
                // convert byte-array to UTF-8 string:
                var convStream =
                    Components.classes["@mozilla.org/intl/converter-input-stream;1"]
                              .createInstance(Components.interfaces.nsIConverterInputStream);
                convStream.init(pipe.inputStream, "UTF-8", 0,
                                Components.interfaces.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
                try {
                    var portion = {};
                    while (convStream.readString(-1, portion)) {
                        html += portion.value;
                    }
                } finally {
                    convStream.close();
                }
            } catch (e) {
                dump("printDialog::refreshHtml:" + e + "\n");
                Components.utils.reportError(e);
            }
            document.getElementById('content').contentDocument.documentElement.innerHTML = html;

            if (finishFunc) {
                finishFunc();
            }
        }
    );
}

function printAndClose()
{
    refreshHtml(
        function finish() {
            PrintUtils.print();
            var closeDialog = true;
#ifdef XP_UNIX
#ifndef XP_MACOSX
            closeDialog = false;
#endif
#endif
            // XXX: printing fails "printing failed while in preview"
            //      if dialog is closed too early on Unix
            if (closeDialog) {
                document.getElementById("calendar-new-printwindow").cancelDialog();
            }
        });
    return false; // leave open
}

/**
 * Called when once a date has been selected in the datepicker.
 */
function onDatePick() {
    calRadioGroupSelectItem("view-field", "custom-range");
    refreshHtml();
}
