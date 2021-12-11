// |reftest| skip-if(!this.hasOwnProperty("Intl"))

var otherGlobal = newGlobal();

var numberFormat = new Intl.NumberFormat();
var ccwNumberFormat = new otherGlobal.Intl.NumberFormat();

// Test Intl.NumberFormat.prototype.format with a CCW object.
var Intl_NumberFormat_format_get = Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, "format").get;

assertEq(Intl_NumberFormat_format_get.call(ccwNumberFormat)(0),
         Intl_NumberFormat_format_get.call(numberFormat)(0));

// Test Intl.NumberFormat.prototype.formatToParts with a CCW object.
var Intl_NumberFormat_formatToParts = Intl.NumberFormat.prototype.formatToParts;

assertEq(deepEqual(Intl_NumberFormat_formatToParts.call(ccwNumberFormat, 0),
                   Intl_NumberFormat_formatToParts.call(numberFormat, 0)),
         true);

// Test Intl.NumberFormat.prototype.resolvedOptions with a CCW object.
var Intl_NumberFormat_resolvedOptions = Intl.NumberFormat.prototype.resolvedOptions;

assertEq(deepEqual(Intl_NumberFormat_resolvedOptions.call(ccwNumberFormat),
                   Intl_NumberFormat_resolvedOptions.call(numberFormat)),
         true);

// Special case for Intl.NumberFormat: The Intl fallback symbol.

function fallbackSymbol(global) {
    var NF = global.Intl.NumberFormat;
    return Object.getOwnPropertySymbols(NF.call(Object.create(NF.prototype)))[0];
}

const intlFallbackSymbol = fallbackSymbol(this);
const otherIntlFallbackSymbol = fallbackSymbol(otherGlobal);
assertEq(intlFallbackSymbol === otherIntlFallbackSymbol, false);

// Test when the fallback symbol points to a CCW NumberFormat object.
var objWithFallbackCCWNumberFormat = {
    __proto__: Intl.NumberFormat.prototype,
    [intlFallbackSymbol]: ccwNumberFormat,
};

assertEq(Intl_NumberFormat_format_get.call(objWithFallbackCCWNumberFormat)(0),
         Intl_NumberFormat_format_get.call(numberFormat)(0));

assertEq(deepEqual(Intl_NumberFormat_resolvedOptions.call(objWithFallbackCCWNumberFormat),
                   Intl_NumberFormat_resolvedOptions.call(numberFormat)),
         true);

// Ensure the fallback symbol(s) are not accessed for CCW NumberFormat objects.
var ccwNumberFormatWithPoisonedFallback = new otherGlobal.Intl.NumberFormat();
Object.setPrototypeOf(ccwNumberFormatWithPoisonedFallback, Intl.NumberFormat.prototype);
Object.defineProperty(ccwNumberFormatWithPoisonedFallback, intlFallbackSymbol, {
    get() { throw new Error(); }
});
Object.defineProperty(ccwNumberFormatWithPoisonedFallback, otherIntlFallbackSymbol, {
    get() { throw new Error(); }
});

assertEq(Intl_NumberFormat_format_get.call(ccwNumberFormatWithPoisonedFallback)(0),
         Intl_NumberFormat_format_get.call(numberFormat)(0));

assertEq(deepEqual(Intl_NumberFormat_resolvedOptions.call(ccwNumberFormatWithPoisonedFallback),
                   Intl_NumberFormat_resolvedOptions.call(numberFormat)),
         true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
