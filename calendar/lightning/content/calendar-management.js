function getCalendarManager()
{
    return Components.classes["@mozilla.org/calendar/manager;1"].getService(Components.interfaces.calICalendarManager);
}

function getCalendars()
{
    return getCalendarManager().getCalendars({});
}

var ltnCalendarTreeView = {
    get rowCount()
    {
        try {
            return getCalendars().length;
        } catch (e) {
            return 0;
        }
    },
    getCellText: function (row, col)
    {
        try {
            return getCalendars()[row].name;
        } catch (e) {
            return "<Unknown " + row + ">";
        }
    },
    setTree: function(treebox) { this.treebox = treebox; },
    isContainer: function(row) { return false; },
    isSeparator: function(row) { return false; },
    isSorted: function(row) { return false; },
    getLevel: function(row) { return 0; },
    getImageSrc: function(row, col) { return null; },
    getRowProperties: function(row, props) { },
    getCellProperties: function(row, col, props) { },
    getColumnProperties: function(colid, col, props) { }
};

function ltnSetTreeView()
{
    document.getElementById("calendarTree").view = ltnCalendarTreeView;
}

window.addEventListener("load", ltnSetTreeView, false);
