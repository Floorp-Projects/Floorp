// |reftest| skip-if(!this.hasOwnProperty("Intl"))

var otherGlobal = newGlobal();

var relativeTimeFormat = new Intl.RelativeTimeFormat();
var ccwRelativeTimeFormat = new otherGlobal.Intl.RelativeTimeFormat();

// Test Intl.RelativeTimeFormat.prototype.format with a CCW object.
var Intl_RelativeTimeFormat_format = Intl.RelativeTimeFormat.prototype.format;

assertEq(Intl_RelativeTimeFormat_format.call(ccwRelativeTimeFormat, 0, "hour"),
         Intl_RelativeTimeFormat_format.call(relativeTimeFormat, 0, "hour"));

// Test Intl.RelativeTimeFormat.prototype.resolvedOptions with a CCW object.
var Intl_RelativeTimeFormat_resolvedOptions = Intl.RelativeTimeFormat.prototype.resolvedOptions;

assertEq(deepEqual(Intl_RelativeTimeFormat_resolvedOptions.call(ccwRelativeTimeFormat),
                   Intl_RelativeTimeFormat_resolvedOptions.call(relativeTimeFormat)),
         true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
