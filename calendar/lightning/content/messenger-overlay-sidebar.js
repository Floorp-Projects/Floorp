function ltnSidebarCalendarSelected(tree)
{
   getCompositeCalendar().defaultCalendar = ltnSelectedCalendar();
}

function ltnSelectedCalendar()
{
    var index = document.getElementById("calendarTree").currentIndex;
    return getCalendars()[index]; 
}

function ltnDeleteSelectedCalendar()
{
    ltnRemoveCalendar(ltnSelectedCalendar());
}

function ltnEditSelectedCalendar()
{
    ltnEditCalendarProperties(ltnSelectedCalendar());
}

function today()
{
    var d = Components.classes['@mozilla.org/calendar/datetime;1'].createInstance(Components.interfaces.calIDateTime);
    d.jsDate = new Date();
    return d.getInTimezone(calendarDefaultTimezone());
}

function nextMonth(dt)
{
    var d = new Date(dt);
    d.setDate(1); // make sure we avoid "June 31" when we bump the month

    var mo = d.getMonth();
    if (mo == 11) {
        d.setMonth(0);
        d.setYear(d.getYear() + 1);
    } else {
        d.setMonth(mo + 1);
    }

    return d;
}

var gMiniMonthLoading = false;
function ltnMinimonthPick(minimonth)
{
    if (gMiniMonthLoading)
        return;

    var cdt = new CalDateTime();
    cdt.jsDate = minimonth.value;
    cdt = cdt.getInTimezone(calendarDefaultTimezone());
    cdt.isDate = true;

    showCalendar(cdt);
}

function ltnOnLoad(event)
{
    gMiniMonthLoading = true;

    var today = new Date();
    var nextmo = nextMonth(today);

    document.getElementById("ltnMinimonth").value = today;

    gMiniMonthLoading = false;

    // nuke the onload, or we get called every time there's
    // any load that occurs
    document.removeEventListener("load", ltnOnLoad, true);

    // Hide the calendar view so it doesn't push the status-bar offscreen
    collapseElement(document.getElementById("calendar-view-box"));

    // Start observing preferences
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService);
    var rootPrefBranch = prefService.getBranch("");
    ltnPrefObserver.rootPrefBranch = rootPrefBranch;
    var pb2 = rootPrefBranch.QueryInterface(
        Components.interfaces.nsIPrefBranch2);
    pb2.addObserver("calendar.", ltnPrefObserver, false);
    ltnPrefObserver.observe(null, null, "");

    // Add an unload function to the window so we don't leak the pref observer
    document.getElementById("messengerWindow")
            .addEventListener("unload", ltnFinish, false);

    // Set up the multiday-view to start/end at the correct hours, since this
    // doesn't persist between startups.  (Fails if pref undefined)
    try {
        var sHour = rootPrefBranch.getIntPref("calendar.event.defaultstarthour");
        var eHour = rootPrefBranch.getIntPref("calendar.event.defaultendhour");
        document.getElementById("calendar-multiday-view")
                .setStartEndMinutes(sHour*60, eHour*60);
    }
    catch(ex) {}

    return;
}

function currentView()
{
    var calendarViewBox = document.getElementById("calendar-view-box");
    return calendarViewBox.selectedPanel;
}

function showCalendar(aDate1, aDate2)
{
    // If we got this call while a mail-view is being shown, we need to
    // hide all of the mail stuff so we have room to display the calendar
    var calendarViewBox = document.getElementById("calendar-view-box");
    if (calendarViewBox.style.visibility == "collapse") {
        collapseElement(GetMessagePane());
        collapseElement(document.getElementById("threadpane-splitter"));
        collapseElement(gSearchBox);
        uncollapseElement(calendarViewBox);
    }

    var view = currentView();

    if (view.displayCalendar != getCompositeCalendar()) {
        view.displayCalendar = getCompositeCalendar();
        view.controller = ltnCalendarViewController;
    }

    if (aDate1 && !aDate2)
        view.showDate(aDate1);
    else if (aDate1 && aDate2)
        view.setDateRange(aDate1, aDate2);

    document.getElementById("displayDeck").selectedPanel =
        document.getElementById("calendar-view-box");
}

function switchView(type) {
    var calendarViewBox = document.getElementById("calendar-view-box");

    var monthView = document.getElementById("calendar-month-view");
    var multidayView = document.getElementById("calendar-multiday-view");

    var selectedDay = calendarViewBox.selectedPanel.selectedDay;

    if (!selectedDay)
        selectedDay = today();

    var d1, d2;

    switch (type) {
    case "month": {
        d1 = selectedDay;
        calendarViewBox.selectedPanel = monthView;
    }
        break;
    case "week": {
        d1 = selectedDay.startOfWeek;
        d2 = selectedDay.endOfWeek;
        calendarViewBox.selectedPanel = multidayView;
    }
        break;
    case "day":
    default: {
        d1 = selectedDay;
        d2 = selectedDay;
        calendarViewBox.selectedPanel = multidayView;
    }
        break;
    }

    showCalendar(d1, d2);

    // Set the labels for the context-menu
    var nextCommand = document.getElementById("context_next");
    nextCommand.setAttribute("label", nextCommand.getAttribute("label-"+type));
    var previousCommand = document.getElementById("context_previous")
    previousCommand.setAttribute("label", previousCommand.getAttribute("label-"+type));

    calendarViewBox.selectedPanel.selectedDay = selectedDay;
}

function selectedCalendarPane(event)
{
    var deck = document.getElementById("displayDeck");

    // If we're already showing a calendar view, don't do anything
    if (deck.selectedPanel.id == "calendar-view-box")
        return;

    deck.selectedPanel = document.getElementById("calendar-view-box");

    switchView('week');
}

function LtnObserveDisplayDeckChange(event)
{
    var deck = event.target;

    // Bug 309505: The 'select' event also fires when we change the selected
    // panel of calendar-view-box.  Workaround with this check.
    if (deck.id != "displayDeck") {
        return;
    }

    var id = null;
    try { id = deck.selectedPanel.id } catch (e) { }

    // Now we're switching back to the mail view, so put everything back that
    // we collapsed in switchView()
    if (id != "calendar-view-box") {
        collapseElement(document.getElementById("calendar-view-box"));
        uncollapseElement(GetMessagePane());
        uncollapseElement(document.getElementById("threadpane-splitter"));
        uncollapseElement(gSearchBox);
    }
}

function ltnPublishCalendar()
{
    currentCalendar = ltnSelectedCalendar();

    openDialog("chrome://calendar/content/calendar-publish-dialog.xul", "caPublishEvents", "chrome,titlebar,modal", currentCalendar);
}

function ltnFinish() {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefService);
    // Remove the pref observer
    var pb2 = prefService.getBranch("");
    pb2 = pb2.QueryInterface(Components.interfaces.nsIPrefBranch2);
    pb2.removeObserver("calendar.", ltnPrefObserver);
    return;
}

function ltnEditSelectedItem() {
    ltnCalendarViewController.modifyOccurrence(currentView().selectedItem);
}

function ltnDeleteSelectedItem() {
    ltnCalendarViewController.deleteOccurrence(currentView().selectedItem);
}

function ltnCreateEvent() {
    ltnCalendarViewController.createNewEvent(ltnSelectedCalendar());
}

// Preference observer, watches for changes to any 'calendar.' pref
var ltnPrefObserver =
{
   rootPrefBranch: null,
   observe: function(aSubject, aTopic, aPrefName)
   {
       switch (aPrefName) {
           case "calendar.event.defaultstarthour":
           case "calendar.event.defaultendhour":
               var sHour = this.rootPrefBranch.getIntPref
                               ("calendar.event.defaultstarthour");
               var eHour = this.rootPrefBranch.getIntPref
                                ("calendar.event.defaultendhour");
               document.getElementById("calendar-multiday-view")
                       .setStartEndMinutes(sHour*60, eHour*60);
               break;
       }
   }
}

function onMouseOverItem(event) {
//set the item's context-menu text here
}

document.getElementById("displayDeck").
    addEventListener("select", LtnObserveDisplayDeckChange, true);

document.addEventListener("load", ltnOnLoad, true);
