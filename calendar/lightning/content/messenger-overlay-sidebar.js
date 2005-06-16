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
function ltnMinimonthPick(which, minimonth)
{
    if (gMiniMonthLoading)
        return;

    if (which == "left") {
        // update right
        var d2 = nextMonth(minimonth.value);
        document.getElementById("ltnMinimonthRight").showMonth(d2);
    }

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

    document.getElementById("ltnMinimonthLeft").value = today;
    document.getElementById("ltnMinimonthRight").showMonth(nextmo);

    gMiniMonthLoading = false;

    // nuke the onload, or we get called every time there's
    // any load that occurs
    document.removeEventListener("load", ltnOnLoad, true);
}

function currentView()
{
    var calendarViewBox = document.getElementById("calendar-view-box");
    return calendarViewBox.selectedPanel;
}

function showCalendar(aDate1, aDate2)
{
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
    var messengerDisplayDeck = document.getElementById("displayDeck");
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

    calendarViewBox.selectedPanel.selectedDay = selectedDay;
}

function selectedCalendarPane(event)
{
    document.getElementById("displayDeck").selectedPanel =
        document.getElementById("calendar-view-box");

    // give the view the calendar
    showCalendar(today());
}

function LtnObserveDisplayDeckChange(event)
{
    var deck = event.target;
    var id = null;
    try { id = deck.selectedPanel.id } catch (e) { }
    if (id == "calendar-view-box") {
        GetMessagePane().collapsed = true;
        document.getElementById("threadpane-splitter").collapsed = true;
        gSearchBox.collapsed = true;
    } else {
        // nothing to "undo" for now
        // Later: mark the view as not needing reflow due to new events coming
        // in, for better performance and batching.
    }
}

function ltnPublishCalendar()
{
    currentCalendar = ltnSelectedCalendar();

    openDialog("chrome://calendar/content/calendar-publish-dialog.xul", "caPublishEvents", "chrome,titlebar,modal", currentCalendar);
}

document.getElementById("displayDeck").
    addEventListener("select", LtnObserveDisplayDeckChange, true);

document.addEventListener("load", ltnOnLoad, true);
