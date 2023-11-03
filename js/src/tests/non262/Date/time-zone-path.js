// |reftest| skip-if(xulRuntime.OS=="WINNT"||!xulRuntime.shell||!this.hasOwnProperty("Intl"))

assertEq(/^(PST|PDT)$/.test(getTimeZone()), true,
         "The default time zone is set to PST8PDT for all jstests (when run in the shell)");

function timeZoneName() {
    var dtf = new Intl.DateTimeFormat("en-US", {timeZoneName: "long"});
    return dtf.formatToParts(Date.UTC(2017, 2, 31, 12, 0, 0)).filter(x => x.type === "timeZoneName")[0].value;
}

setTimeZone("Europe/Paris");
assertEq(timeZoneName(), "Central European Summer Time");

setTimeZone(":Europe/Helsinki");
assertEq(timeZoneName(), "Eastern European Summer Time");

setTimeZone("/zoneinfo/America/Chicago");
assertEq(timeZoneName(), "Central Daylight Time");

setTimeZone("/this-part-is-ignored/zoneinfo/America/Chicago");
assertEq(timeZoneName(), "Central Daylight Time");

setTimeZone(":/this-part-is-ignored/zoneinfo/America/Phoenix");
assertEq(timeZoneName(), "Mountain Standard Time");

const invalidTimeZones = [
    // Non-existent timezone
    "foo",
    "/zoneinfo/foo",
    "/zoneinfo/",

    // Not capitalized
    "america/chicago",

    // No leading /
    "zoneinfo/America/Chicago",
    "foo/zoneinfo/America/Chicago",
    ":zoneinfo/America/Chicago",
    ":foo/zoneinfo/America/Chicago",

    // Path without "/zoneinfo/"
    "/foo/America/Chicago",
    ":/foo/America/Chicago",

    // Two colons
    "::Europe/London",
    "::/zoneinfo/America/Los_Angeles",
    "::/this-part-is-ignored/zoneinfo/America/Los_Angeles",
];

for (const invalid of invalidTimeZones) {
    assertThrowsInstanceOf(() => setTimeZone(invalid), Error);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
