if (typeof getDefaultLocale === "undefined") {
    var getDefaultLocale = SpecialPowers.Cu.getJSTestingFunctions().getDefaultLocale;
}
if (typeof setDefaultLocale === "undefined") {
    var setDefaultLocale = SpecialPowers.Cu.getJSTestingFunctions().setDefaultLocale;
}
if (typeof getTimeZone === "undefined") {
    var getTimeZone = SpecialPowers.Cu.getJSTestingFunctions().getTimeZone;
}
if (typeof setTimeZone === "undefined") {
    var setTimeZone = SpecialPowers.Cu.getJSTestingFunctions().setTimeZone;
}
