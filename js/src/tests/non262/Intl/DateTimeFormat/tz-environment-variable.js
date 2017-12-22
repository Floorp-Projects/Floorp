// |reftest| skip-if(!this.hasOwnProperty("Intl")||(xulRuntime.OS=="WINNT"&&!xulRuntime.shell)) -- Windows browser in automation doesn't pick up new time zones correctly

// From bug 1330149:
//
// Windows only supports a very limited set of IANA time zone names for the TZ
// environment variable.
//
// TZ format supported by Windows: "TZ=tzn[+|-]hh[:mm[:ss]][dzn]".
//
// Complete list of all IANA time zone ids matching that format.
//
// From tzdata's "northamerica" file:
//   EST5EDT
//   CST6CDT
//   MST7MDT
//   PST8PDT
//
// From tzdata's "backward" file:
//   GMT+0
//   GMT-0
//   GMT0
//
// Also supported on Windows even though they don't match the format listed
// above.
//
// From tzdata's "backward" file:
//   UCT
//   UTC
//
// From tzdata's "etcetera" file:
//   GMT

function inTimeZone(tzname, fn) {
    setTimeZone(tzname);
    try {
        fn();
    } finally {
        setTimeZone("PST8PDT");
    }
}

const timeZones = [
    { id: "EST5EDT" },
    { id: "CST6CDT" },
    { id: "MST7MDT" },
    { id: "PST8PDT" },
    // ICU on non-Windows platforms doesn't accept these three time zone
    // identifiers, cf. isValidOlsonID in $ICU/source/common/putil.cpp. We
    // could add support for them, but it seems unlikely they're used in
    // practice, so we just skip over them.
    // { id: "GMT+0", normalized: "UTC" },
    // { id: "GMT-0", normalized: "UTC" },
    // { id: "GMT0", normalized: "UTC" },
    { id: "UCT", normalized: "Etc/UCT" },
    { id: "UTC", normalized: "UTC" },
    { id: "GMT", normalized: "UTC" },
];

for (let {id, normalized = id} of timeZones) {
    inTimeZone(id, () => {
        let opts = new Intl.DateTimeFormat().resolvedOptions();
        assertEq(opts.timeZone, normalized);
    });
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
