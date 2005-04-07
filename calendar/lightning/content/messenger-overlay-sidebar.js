function selectedCalendarPane(event)
{
    dump("selecting calendar pane\n");
    document.getElementById("displayDeck").selectedPanel =
        document.getElementById("calendarViewBox");
}

function LtnObserveDisplayDeckChange(event)
{
    var deck = event.target;
    if (deck.selectedPanel.id == "calendarViewBox") {
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