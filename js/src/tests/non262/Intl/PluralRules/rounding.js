// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// The rounding mode defaults to half-up for both NumberFormat and PluralRules.

var locale = "en";
var options = {maximumFractionDigits: 0};

assertEq(new Intl.NumberFormat(locale, options).format(0), "0");
assertEq(new Intl.NumberFormat(locale, options).format(0.5), "1");
assertEq(new Intl.NumberFormat(locale, options).format(1), "1");

assertEq(new Intl.PluralRules(locale, options).select(0), "other");
assertEq(new Intl.PluralRules(locale, options).select(0.5), "one");
assertEq(new Intl.PluralRules(locale, options).select(1), "one");

if (typeof reportCompare === "function")
    reportCompare(0, 0, 'ok');
