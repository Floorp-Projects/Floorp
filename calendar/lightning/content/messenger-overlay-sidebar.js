function selectedCalendarPane(event)
{
    dump("selecting calendar pane\n");
    document.getElementById("displayDeck").selectedPanel =
        document.getElementById("calendar-view-box");

    // give the view the calendar
    var view = document.getElementById("calendar-multiday-view");
    if (view.displayCalendar != getCompositeCalendar()) {
        var d = Components.classes['@mozilla.org/calendar/datetime;1'].createInstance(Components.interfaces.calIDateTime);
        d.jsDate = new Date();
        //XXX use default timezone
        d = d.getInTimezone("/mozilla.org/20050126_1/America/Los_Angeles");
        var st = d.startOfWeek;
        var end = d.endOfWeek;

        view.setDateRange(st, end);
        view.displayCalendar = getCompositeCalendar();
        view.controller = ltnCalendarViewController;
    }
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

document.getElementById("displayDeck").
    addEventListener("select", LtnObserveDisplayDeckChange, true);
