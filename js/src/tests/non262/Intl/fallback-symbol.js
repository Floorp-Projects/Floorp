// |reftest| skip-if(!this.hasOwnProperty('Intl'))

function IntlFallbackSymbol(constructor) {
    return Object.getOwnPropertySymbols(constructor.call(Object.create(constructor.prototype)))[0];
}

const dateTimeFormatIntlFallbackSymbol = IntlFallbackSymbol(Intl.DateTimeFormat);
const numberFormatIntlFallbackSymbol = IntlFallbackSymbol(Intl.NumberFormat);

// Intl.DateTimeFormat and Intl.NumberFormat both use the same fallback symbol.
assertEq(dateTimeFormatIntlFallbackSymbol, numberFormatIntlFallbackSymbol);

const intlFallbackSymbol = dateTimeFormatIntlFallbackSymbol;

// The fallback symbol is a Symbol value.
assertEq(typeof intlFallbackSymbol, "symbol");

// Test the description of the fallback symbol.
assertEq(Symbol.prototype.toString.call(intlFallbackSymbol), "Symbol(IntlLegacyConstructedSymbol)");

if (typeof reportCompare === "function")
    reportCompare(true, true);
