// Agenda tree view, to display upcoming events, tasks, and reminders
//
// We track three periods of time for a segmented view:
// - today: the current time until midnight
// - tomorrow: midnight to midnight
// - soon: end-of-tomorrow to end-of-one-week-from-today (midnight)
//
// Events (recurrences of events, really) are stored in per-period containers,
// hung off of "period" objects. In addition, we build an array of the row-
// representation we use for backing the tree display.
//
// The tree-view array (this.events) consists of the synthetic events for the time
// periods, each one followed, if tree-expanded, by its collection of events.  This
// results in a this.events array like the following, if "Today" and "Soon" are
// expanded:
// [ synthetic("Today"),
//   occurrence("Today Event 1"),
//   occurrence("Today Event 2"),
//   synthetic("Tomorrow"),
//   synthetic("Soon"),
//   occurrence("Soon Event 1"),
//   occurrence("Soon Event 2") ]
//
// At window load, we connect the view to the tree and initiate a calendar query
// to populate the event buckets.  Once the query is complete, we sort each bucket
// and then build the aggregate array described above.
//
// When calendar queries are refreshed (by a calendar being added/removed WRT the
// current view, the user selecting a different filter, or some hidden manual-
// refresh testing UI) the event buckets are emptied, and we add items as they
// arrive.
//
// This is currently a localization disaster.  Quel dommage!

function Synthetic(title, open)
{
    this.title = title;
    this.open = open;
    this.events = [];
}

Synthetic.prototype.toString =
function toString()
{
    return "[Synthetic: " + this.title + "/" + (this.open ? "open" : "closed") + "]";
};

var agendaTreeView = {
    // This is the first time I've used sharp variables in earnest!
    today: #1=(new Synthetic("Today", true)),
    tomorrow: #2=(new Synthetic("Tomorrow", false)),
    soon: #3=(new Synthetic("Soon", false)),
    periods: [#1#, #2#, #3#],
    events: [],
    todayCount: 0,
    tomorrowCount: 0,
    soonCount: 0,
    prevRowCount: 0
};

agendaTreeView.addEvents =
function addEvents(master)
{
    this.events.push(master);
    if (master.open)
        this.events = this.events.concat(master.events);
};

agendaTreeView.rebuildEventsArray =
function rebuildEventsArray()
{
    this.events = [];
    this.addEvents(this.today);
    this.addEvents(this.tomorrow);
    this.addEvents(this.soon);
};

agendaTreeView.forceTreeRebuild =
function forceTreeRebuild()
{
    if (this.tree) {
        // dump("forcing tree rebuild\n");
        this.tree.view = this;
    }
};

agendaTreeView.rebuildAgendaView =
function rebuildAgendaView(invalidate)
{
    this.rebuildEventsArray();
/*
    dump("events:\n");
    this.events.forEach(function (e) {
        if (e instanceof Synthetic)
            dump("  " + e.title + "\n");
        else
            dump("    " + e.title + " @ " + e.occurrenceStartDate + "\n");
    });
*/
    this.forceTreeRebuild();
};

agendaTreeView.__defineGetter__("rowCount",
function get_rowCount()
{
    //dump("row count: " + this.events.length + "\n");
    return this.events.length;
});

agendaTreeView.isContainer =
function isContainer(row)
{
    // dump("row " + row + " is " + this.events[row] + "\n")
    return (this.events[row] instanceof Synthetic);
};

agendaTreeView.isContainerOpen =
function isContainerOpen(row)
{
    var open = this.events[row].open;
    // dump("row " + row + " (" + this.events[row].title + ") is " + (open ? "open" : "closed") + "\n");
    return open;
};

agendaTreeView.isContainerEmpty =
function isContainerEmpty(row)
{
    if (this.events[row].events.length == 0)
        return true;
    return false;
};

agendaTreeView.setTree =
function setTree(tree)
{
    this.tree = tree;
};

agendaTreeView.getCellText =
function getCellText(row, column)
{
    var event = this.events[row];
    if (column.id == "col-agenda-item") {
        if (event instanceof Synthetic)
            return event.title;
        return event.title;
    }

    if (event instanceof Synthetic)
        return "";
    var start = event.startDate || event.entryDate;
    return start.toString();
};

agendaTreeView.getLevel =
function getLevel(row)
{
    if (this.isContainer(row))
        return 0;
    return 1;
};

agendaTreeView.isSorted =
function isSorted() { return false; };

agendaTreeView.isEditable =
function isEditable(row, column) { return false; };

agendaTreeView.isSeparator =
function isSeparator(row) { return false; };

agendaTreeView.getImageSrc =
function getImageSrc(row, column) { return null; };

agendaTreeView.getCellProperties =
function getCellProperties(row, column) { return null; };

agendaTreeView.getRowProperties =
function getRowProperties(row) { return null; };

agendaTreeView.getColumnProperties =
function getColumnProperties(column) { return null; };

agendaTreeView.cycleHeader =
function cycleHeader(header)
{
    this.refreshCalendarQuery(); // temporary hackishness
    this.rebuildAgendaView();
    this.forceTreeRebuild();
};

agendaTreeView.getParentIndex =
function getParentIndex(row)
{
    if (this.isContainer(row))
        return -1;
    var i = row - 1;
    do {
        if (this.events[i] instanceof Synthetic)
            return i;
        i--;
    } while (i != 0);
    throw "no parent for row " + row + "?";
};

agendaTreeView.toggleOpenState =
function toggleOpenState(row)
{
    if (!this.isContainer(row))
        throw "toggling open state on non-container row " + row + "?";
    var header = this.events[row];
    if (!("open") in header)
        throw "no open state found on container row " + row + "?";
    header.open = !header.open;
    this.rebuildAgendaView(); // reconstruct the visible row set
    this.forceTreeRebuild();
};

agendaTreeView.hasNextSibling =
function hasNextSibling(row, afterIndex)
{
};

agendaTreeView.findPeriodForItem =
function findPeriodForItem(item)
{
    var start = item.startDate || item.entryDate;
    if (start.compare(this.today.end) <= 0)
        return this.today;
        
    if (start.compare(this.tomorrow.end) <= 0)
        return this.tomorrow;
    
    if (start.compare(this.soon.end) <= 0)
        return this.soon;
    
    void(item.title + " @ " + start + " not in range " +
         "(" + this.today.start + " - " + this.soon.end + ")\n");

    return null;
};

agendaTreeView.addItem =
function addItem(item)
{
    var when = this.findPeriodForItem(item);
    if (!when)
        return;
    void(item.title + " @ " + item.occurrenceStartDate + " -> " + when.title + "\n");
    when.events.push(item);
    this.calendarUpdateComplete();
};

agendaTreeView.deleteItem =
function deleteItem(item)
{
    var when = this.findPeriodForItem(item);
    if (!when) {
        void("deleting non-binned item " + item + "\n");
        return;
    }
    
    void("deleting item " + item + " from " + when.title + "\n");
    void("before: " + when.events.map(function (e) { return e.title; }).join(" ") + "\n");
    when.events = when.events.filter(function (e) {
                                         if (e.id != item.id)
                                             return true;
                                         if (e.recurrenceId && item.recurrenceId &&
                                             e.recurrenceId.compare(item.recurrenceId) != 0)
                                             return true;
                                         return false;
                                     });
    void("after: " + when.events.map(function (e) { return e.title; }).join(" ") + "\n");
    this.rebuildAgendaView(true);
};

agendaTreeView.calendarUpdateComplete =
function calendarUpdateComplete()
{
    [this.today, this.tomorrow, this.soon].forEach(function(when) {
        function compare(a, b) {
            var ad = a.startDate || a.entryDate;
            var bd = b.startDate || b.entryDate;
            return ad.compare(bd);
        }
        when.events.sort(compare);
    });
    this.rebuildAgendaView(true);
};

agendaTreeView.calendarOpListener =
{
    agendaTreeView: agendaTreeView
};

agendaTreeView.calendarOpListener.onOperationComplete =
function listener_onOperationComplete(calendar, status, optype, id,
                                      detail)
{
    this.agendaTreeView.calendarUpdateComplete();  
};

agendaTreeView.calendarOpListener.onGetResult =
function listener_onGetResult(calendar, status, itemtype, detail, count, items)
{
    if (!Components.isSuccessCode(status))
        return;
    
    items.forEach(this.agendaTreeView.addItem, this.agendaTreeView);
};

agendaTreeView.refreshCalendarQuery =
function refreshCalendarQuery()
{
    var filter = this.calendar.ITEM_FILTER_TYPE_EVENT |
                 this.calendar.ITEM_FILTER_COMPLETED_ALL |
                 this.calendar.ITEM_FILTER_CLASS_OCCURRENCES;

    this.periods.forEach(function (p) { p.events = []; });
    this.calendar.getItems(filter, 0, this.today.start, this.soon.end,
                           this.calendarOpListener);
    void("Calendar query started (" + this.today.start + " -> " + this.soon.end + ")\n");
};

agendaTreeView.refreshPeriodDates =
function refreshPeriodDates()
{
    var now = new Date();
    var d = new CalDateTime();
    d.jsDate = now;
    //XXX use default timezone
    d = d.getInTimezone("/mozilla.org/20050126_1/America/Los_Angeles");

    // Today: now until midnight of tonight
    this.today.start = d.clone();
    d.hour = d.minute = d.second = 0;
    d.day++;
    d.normalize();
    this.today.end = d.clone();

    // Tomorrow: midnight of next day to +24 hrs
    this.tomorrow.start = d.clone();
    d.day++;
    d.normalize();
    this.tomorrow.end = d.clone();

    // Soon: end of tomorrow to 6 six days later (remainder of the week period)
    this.soon.start = d.clone();
    d.day += 6;
    d.normalize();
    this.soon.end = d.clone();

    this.periods.forEach(function (when) {
        void(when.title + ": " + when.start + " -> " + when.end + "\n");
    });
    this.refreshCalendarQuery();
};

agendaTreeView.calendarObserver = {
    agendaTreeView: agendaTreeView
};

agendaTreeView.calendarObserver.onStartBatch = function() {};
agendaTreeView.calendarObserver.onEndBatch = function() {};
agendaTreeView.calendarObserver.onStartLoad = function() {};

agendaTreeView.calendarObserver.onAddItem =
function observer_onAddItem(item)
{
    if (!(item instanceof Components.interfaces.calIEvent))
        return;

    var occs = item.getOccurrencesBetween(this.agendaTreeView.today.start,
                                          this.agendaTreeView.soon.end, {});
    occs.forEach(this.agendaTreeView.addItem, this.agendaTreeView);
    this.agendaTreeView.rebuildAgendaView();
};

agendaTreeView.calendarObserver.onDeleteItem =
function observer_onDeleteItem(item, rebuildFlag)
{
    if (!(item instanceof Components.interfaces.calIEvent))
        return;

    var occs = item.getOccurrencesBetween(this.agendaTreeView.today.start,
                                          this.agendaTreeView.soon.end, {});
    occs.forEach(this.agendaTreeView.deleteItem, this.agendaTreeView);
    if (rebuildFlag != "no-rebuild")
        this.agendaTreeView.rebuildAgendaView();
};

agendaTreeView.calendarObserver.onModifyItem =
function observer_onModifyItem(newItem, oldItem)
{
    this.onDeleteItem(oldItem, "no-rebuild");
    this.onAddItem(newItem);
};

agendaTreeView.setCalendar =
function setCalendar(calendar)
{
    void("periods: " + this.periods + "\n");
    if (this.calendar)
        this.calendar.removeObserver(this.calendarObserver);
    this.calendar = calendar;
    calendar.addObserver(this.calendarObserver);

    // Update everything
    this.refreshPeriodDates();
};

function setAgendaTreeView()
{
    agendaTreeView.setCalendar(getCompositeCalendar());
    document.getElementById("agenda-tree").view = agendaTreeView;
}

window.addEventListener("load", setAgendaTreeView, false);
