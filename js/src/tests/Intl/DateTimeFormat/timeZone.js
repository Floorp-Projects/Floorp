// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const tzMapper = [
    x => x,
    x => x.toUpperCase(),
    x => x.toLowerCase(),
];


const utcTimeZones = [
    // Etc/UTC and Etc/GMT are normalized to UTC.
    "Etc/UTC", "Etc/GMT",

    // Links to Etc/GMT. (tzdata/etcetera)
    "GMT", "Etc/Greenwich", "Etc/GMT-0", "Etc/GMT+0", "Etc/GMT0",

    // Links to Etc/UTC. (tzdata/etcetera)
    "Etc/Universal", "Etc/Zulu",

    // Links to Etc/GMT. (tzdata/backward)
    "GMT+0", "GMT-0", "GMT0", "Greenwich",

    // Links to Etc/UTC. (tzdata/backward)
    "UTC", "Universal", "Zulu",
];

for (let timeZone of utcTimeZones) {
    for (let map of tzMapper) {
        let dtf = new Intl.DateTimeFormat(undefined, {timeZone: map(timeZone)});
        assertEq(dtf.resolvedOptions().timeZone, "UTC");
    }
}


// ECMA-402 doesn't normalize Etc/UCT to UTC.
const uctTimeZones = [
    "Etc/UCT", "UCT",
];

for (let timeZone of uctTimeZones) {
    for (let map of tzMapper) {
        let dtf = new Intl.DateTimeFormat(undefined, {timeZone: map(timeZone)});
        assertEq(dtf.resolvedOptions().timeZone, "Etc/UCT");
    }
}


const invalidTimeZones = [
    "", "null", "undefined", "UTC\0",

    // ICU time zone name for invalid time zones.
    "Etc/Unknown",

    // ICU custom time zones.
    "GMT-1", "GMT+1", "GMT-10", "GMT+10",
    "GMT-10:00", "GMT+10:00",
    "GMT-1000", "GMT+1000",

    // Legacy ICU time zones.
    "ACT", "AET", "AGT", "ART", "AST", "BET", "BST", "CAT", "CNT", "CST",
    "CTT", "EAT", "ECT", "IET", "IST", "JST", "MIT", "NET", "NST", "PLT",
    "PNT", "PRT", "PST", "SST", "VST",

    // Deprecated IANA time zones.
    "SystemV/AST4ADT", "SystemV/EST5EDT", "SystemV/CST6CDT", "SystemV/MST7MDT",
    "SystemV/PST8PDT", "SystemV/YST9YDT", "SystemV/AST4", "SystemV/EST5",
    "SystemV/CST6", "SystemV/MST7", "SystemV/PST8", "SystemV/YST9", "SystemV/HST10",
];

for (let timeZone of invalidTimeZones) {
    for (let map of tzMapper) {
        assertThrowsInstanceOf(() => {
            new Intl.DateTimeFormat(undefined, {timeZone: map(timeZone)});
        }, RangeError);
    }
}


// GMT[+-]hh is invalid, but Etc/GMT[+-]hh is a valid IANA time zone.
for (let gmtOffset = -14; gmtOffset <= 12; ++gmtOffset) {
    // Skip Etc/GMT0.
    if (gmtOffset === 0)
        continue;

    let timeZone = `Etc/GMT${gmtOffset > 0 ? "+" : ""}${gmtOffset}`;
    for (let map of tzMapper) {
        let dtf = new Intl.DateTimeFormat(undefined, {timeZone: map(timeZone)});
        assertEq(dtf.resolvedOptions().timeZone, timeZone);
    }
}


const invalidEtcGMTNames = [
  // Out of bounds GMT offset.
  "Etc/GMT-15", "Etc/GMT+13",

  // Etc/GMT[+-]hh:mm isn't a IANA time zone name.
  "Etc/GMT-10:00", "Etc/GMT+10:00",
  "Etc/GMT-1000", "Etc/GMT+1000",
];

for (let timeZone of invalidEtcGMTNames) {
    for (let map of tzMapper) {
        assertThrowsInstanceOf(() => {
            new Intl.DateTimeFormat(undefined, {timeZone: map(timeZone)});
        }, RangeError);
    }
}


// RangeError is thrown for primitive values, because ToString(<primitive>)
// isn't a valid time zone name.
for (let nonStrings of [null, 0, 0.5, true, false]) {
    assertThrowsInstanceOf(() => {
        new Intl.DateTimeFormat(undefined, {timeZone: nonStrings});
    }, RangeError);
}

// ToString(<symbol>) throws TypeError.
assertThrowsInstanceOf(() => {
    new Intl.DateTimeFormat(undefined, {timeZone: Symbol()});
}, TypeError);

// |undefined| or absent "timeZone" option selects the default time zone.
{
    let {timeZone: tzAbsent} = new Intl.DateTimeFormat(undefined, {}).resolvedOptions();
    let {timeZone: tzUndefined} = new Intl.DateTimeFormat(undefined, {timeZone: undefined}).resolvedOptions();

    assertEq(typeof tzAbsent, "string");
    assertEq(typeof tzUndefined, "string");
    assertEq(tzUndefined, tzAbsent);

    // The default time zone isn't a link name.
    let {timeZone: tzDefault} = new Intl.DateTimeFormat(undefined, {timeZone: tzAbsent}).resolvedOptions();
    assertEq(tzDefault, tzAbsent);
}

// Objects are converted through ToString().
{
    let timeZone = "Europe/Warsaw";
    let obj = {
        toString() {
            return timeZone;
        }
    };
    let dtf = new Intl.DateTimeFormat(undefined, {timeZone: obj});
    assertEq(dtf.resolvedOptions().timeZone, timeZone);
}


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
