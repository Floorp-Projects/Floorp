// |reftest| skip-if(xulRuntime.OS=="WINNT"||!xulRuntime.shell)

assertEq(/^(PST|PDT)$/.test(getTimeZone()), true,
         "The default time zone is set to PST8PDT for all jstests (when run in the shell)");

function timeZoneName() {
    var dtf = new Intl.DateTimeFormat("en-US", {timeZoneName: "long"});
    return dtf.formatToParts().filter(x => x.type === "timeZoneName")[0].value;
}

// Calling setTimeZone() with an undefined argument clears the TZ environment
// variable and by that reveal the actual system time zone.
setTimeZone(undefined);
var systemTimeZone = getTimeZone();
var systemTimeZoneName = timeZoneName();

// Set to an unlikely system time zone, so that the next call to setTimeZone()
// will lead to a time zone change.
setTimeZone("Antarctica/Troll");

// Now call with the file path ":/etc/localtime" which is special-cased in
// DateTimeInfo to read the system time zone.
setTimeZone(":/etc/localtime");

assertEq(getTimeZone(), systemTimeZone);
assertEq(timeZoneName(), systemTimeZoneName);

if (typeof reportCompare === "function")
    reportCompare(true, true);
