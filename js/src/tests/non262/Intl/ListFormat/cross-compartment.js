// |reftest| skip-if(!this.hasOwnProperty('Intl'))

var g = newGlobal();

var locale = "en";
var list = ["a", "b", "c"];

var listFormat = new Intl.ListFormat(locale);
var ccwListFormat = new g.Intl.ListFormat(locale);

// Intl.ListFormat.prototype.format
{
    var fn = Intl.ListFormat.prototype.format;

    var expectedValue = fn.call(listFormat, list);
    var actualValue = fn.call(ccwListFormat, list);

    assertEq(actualValue, expectedValue);
}

// Intl.ListFormat.prototype.formatToParts
{
    var fn = Intl.ListFormat.prototype.formatToParts;

    var expectedValue = fn.call(listFormat, list);
    var actualValue = fn.call(ccwListFormat, list);

    assertDeepEq(actualValue, expectedValue);
}

// Intl.ListFormat.prototype.resolvedOptions
{
    var fn = Intl.ListFormat.prototype.resolvedOptions;

    var expectedValue = fn.call(listFormat);
    var actualValue = fn.call(ccwListFormat);

    assertDeepEq(actualValue, expectedValue);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
