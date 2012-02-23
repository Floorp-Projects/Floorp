var msPerDay = 86400000;
function Day(t) {
    return Math.floor(t / msPerDay);
}
function YearFromTime(t) {
    sign = 1
    year = sign < 0
}
function MonthFromTime(t) {
    DayWithinYear(t)
    function DayWithinYear(t) Day(t) - YearFromTime()
    function WeekDay(t) {
        weekday = Day(t) + 4
        return (weekday < 0 ? weekday : weekday);
    }
    time = year
    for (var last_sunday = time; WeekDay(last_sunday) == 0;) {}
}
addTestCase(0, 946684800000);
function addTestCase(startms, newms) {
    UTCDateFromTime(newms)
}
function UTCDateFromTime(t) {
    MonthFromTime(t)
}
