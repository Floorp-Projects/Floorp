// |reftest| skip-if(!this.hasOwnProperty("Intl"))

var otherGlobal = newGlobal();

var dateTimeFormat = new Intl.DateTimeFormat();
var ccwDateTimeFormat = new otherGlobal.Intl.DateTimeFormat();

// Test Intl.DateTimeFormat.prototype.format with a CCW object.
var Intl_DateTimeFormat_format_get = Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype, "format").get;

assertEq(Intl_DateTimeFormat_format_get.call(ccwDateTimeFormat)(0),
         Intl_DateTimeFormat_format_get.call(dateTimeFormat)(0));

// Test Intl.DateTimeFormat.prototype.formatToParts with a CCW object.
var Intl_DateTimeFormat_formatToParts = Intl.DateTimeFormat.prototype.formatToParts;

assertEq(deepEqual(Intl_DateTimeFormat_formatToParts.call(ccwDateTimeFormat, 0),
                   Intl_DateTimeFormat_formatToParts.call(dateTimeFormat, 0)),
         true);

// Test Intl.DateTimeFormat.prototype.resolvedOptions with a CCW object.
var Intl_DateTimeFormat_resolvedOptions = Intl.DateTimeFormat.prototype.resolvedOptions;

assertEq(deepEqual(Intl_DateTimeFormat_resolvedOptions.call(ccwDateTimeFormat),
                   Intl_DateTimeFormat_resolvedOptions.call(dateTimeFormat)),
         true);

// Special case for Intl.DateTimeFormat: The Intl fallback symbol.

function fallbackSymbol(global) {
    var DTF = global.Intl.DateTimeFormat;
    return Object.getOwnPropertySymbols(DTF.call(Object.create(DTF.prototype)))[0];
}

const intlFallbackSymbol = fallbackSymbol(this);
const otherIntlFallbackSymbol = fallbackSymbol(otherGlobal);
assertEq(intlFallbackSymbol === otherIntlFallbackSymbol, false);

// Test when the fallback symbol points to a CCW DateTimeFormat object.
var objWithFallbackCCWDateTimeFormat = {
    __proto__: Intl.DateTimeFormat.prototype,
    [intlFallbackSymbol]: ccwDateTimeFormat,
};

assertEq(Intl_DateTimeFormat_format_get.call(objWithFallbackCCWDateTimeFormat)(0),
         Intl_DateTimeFormat_format_get.call(dateTimeFormat)(0));

assertEq(deepEqual(Intl_DateTimeFormat_resolvedOptions.call(objWithFallbackCCWDateTimeFormat),
                   Intl_DateTimeFormat_resolvedOptions.call(dateTimeFormat)),
         true);

// Ensure the fallback symbol(s) are not accessed for CCW DateTimeFormat objects.
var ccwDateTimeFormatWithPoisonedFallback = new otherGlobal.Intl.DateTimeFormat();
Object.setPrototypeOf(ccwDateTimeFormatWithPoisonedFallback, Intl.DateTimeFormat.prototype);
Object.defineProperty(ccwDateTimeFormatWithPoisonedFallback, intlFallbackSymbol, {
    get() { throw new Error(); }
});
Object.defineProperty(ccwDateTimeFormatWithPoisonedFallback, otherIntlFallbackSymbol, {
    get() { throw new Error(); }
});

assertEq(Intl_DateTimeFormat_format_get.call(ccwDateTimeFormatWithPoisonedFallback)(0),
         Intl_DateTimeFormat_format_get.call(dateTimeFormat)(0));

assertEq(deepEqual(Intl_DateTimeFormat_resolvedOptions.call(ccwDateTimeFormatWithPoisonedFallback),
                   Intl_DateTimeFormat_resolvedOptions.call(dateTimeFormat)),
         true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
