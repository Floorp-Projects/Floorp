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
    currentView().showDate(cdt);

    showCalendar(false);
}

function ltnOnLoad(event)
{
    gMiniMonthLoading = true;

    var today = new Date();
    var nextmo = nextMonth(today);

    document.getElementById("ltnMinimonthLeft").value = today;
    document.getElementById("ltnMinimonthRight").showMonth(nextmo);

    gMiniMonthLoading = false;
}

function currentView()
{
    var calendarViewBox = document.getElementById("calendar-view-box");
    return calendarViewBox.selectedPanel;
}

function showCalendar(jumpToToday)
{
    document.getElementById("displayDeck").selectedPanel =
        document.getElementById("calendar-view-box");

    var view = currentView();
    if (jumpToToday) {
        var d = Components.classes['@mozilla.org/calendar/datetime;1'].createInstance(Components.interfaces.calIDateTime);
        d.jsDate = new Date();
        d = d.getInTimezone(calendarDefaultTimezone());
        view.showDate(d);
    }

    if (view.displayCalendar != getCompositeCalendar()) {
        view.displayCalendar = getCompositeCalendar();
        view.controller = ltnCalendarViewController;
    }
}

function switchView(type) {
    var messengerDisplayDeck = document.getElementById("displayDeck");
    var calendarViewBox = document.getElementById("calendar-view-box");

    var monthView = document.getElementById("calendar-month-view");
    var multidayView = document.getElementById("calendar-multiday-view");

    // XXX we need a selectedDate in calICalendarView.idl!
    var selectedDate = calendarViewBox.selectedPanel.startDate;

    if (!selectedDate) {
        var d = Components.classes['@mozilla.org/calendar/datetime;1'].createInstance(Components.interfaces.calIDateTime);
        d.jsDate = new Date();
        selectedDate = d.getInTimezone(calendarDefaultTimezone());
    }

    switch (type) {
    case "month": {
        monthView.showDate(selectedDate);
        calendarViewBox.selectedPanel = monthView;
    }
        break;
    case "week": {
        var start = selectedDate.startOfWeek;
        var end = selectedDate.endOfWeek.clone();
        end.day += 1;
        end.normalize();
        multidayView.setDateRange(start, end);
        calendarViewBox.selectedPanel = multidayView;
    }
        break;
    case "day":
    default: {
        var start = selectedDate;
        var end = selectedDate.clone();
        end.day += 1;
        end.normalize();
        multidayView.setDateRange(start, end);
        calendarViewBox.selectedPanel = multidayView;
    }
        break;
    }

    if (messengerDisplayDeck.selectedPanel = calendarViewBox)
        showCalendar(false);
    else
        showCalendar(true);
}

function selectedCalendarPane(event)
{
    dump("selecting calendar pane\n");
    document.getElementById("displayDeck").selectedPanel =
        document.getElementById("calendar-view-box");

    // give the view the calendar
    showCalendar(true);
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
