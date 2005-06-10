function ltnSidebarCalendarSelected(tree)
{
    var disabled = tree.view.selection.count == 0;
    document.getElementById("cal-sidebar-edit-calendar").disabled = disabled;
    document.getElementById("cal-sidebar-delete-calendar").disabled = disabled;
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
    cdt.isDate = true;
    cdt.timezone = calendarDefaultTimezone();
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
    return document.getElementById("calendar-multiday-view");
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
