// |reftest| skip-if(!this.hasOwnProperty("Intl"))

if (typeof getBuildConfiguration === "undefined") {
  var getBuildConfiguration = SpecialPowers.Cu.getJSTestingFunctions().getBuildConfiguration;
}

var isNightly = !getBuildConfiguration().release_or_beta;

var log;
var proxy = new Proxy({
    year: "numeric",
    hour: "numeric",
}, new Proxy({
    get(t, pk, r) {
        log.push(pk);
        return Reflect.get(t, pk, r);
    }
}, {
    get(t, pk, r) {
        assertEq(pk, "get");
        return Reflect.get(t, pk, r);
    }
}));

var fractionalSecondDigits = isNightly ? ["fractionalSecondDigits"] : [];

var constructorAccesses = [
    // ToDateTimeOptions(options, "any", "date").
    "weekday", "year", "month", "day",
    "hour", "minute", "second", ...fractionalSecondDigits,

    // InitializeDateTimeFormat
    "localeMatcher", "calendar", "numberingSystem", "hour12", "hourCycle", "timeZone",

    // Table 5: Components of date and time formats
    "weekday", "era", "year", "month", "day", "hour", "minute", "second", "timeZoneName",

    // InitializeDateTimeFormat
    ...fractionalSecondDigits,
    "formatMatcher",
];

log = [];
new Intl.DateTimeFormat(undefined, proxy);

assertEqArray(log, constructorAccesses);

log = [];
new Date().toLocaleString(undefined, proxy);

assertEqArray(log, [
    // ToDateTimeOptions(options, "any", "all").
    "weekday", "year", "month", "day",
    "hour", "minute", "second", ...fractionalSecondDigits,

    ...constructorAccesses
]);

log = [];
new Date().toLocaleDateString(undefined, proxy);

assertEqArray(log, [
    // ToDateTimeOptions(options, "date", "date").
    "weekday", "year", "month", "day",

    ...constructorAccesses
]);

log = [];
new Date().toLocaleTimeString(undefined, proxy);

assertEqArray(log, [
    // ToDateTimeOptions(options, "time", "time").
    "hour", "minute", "second", ...fractionalSecondDigits,

    ...constructorAccesses
]);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
