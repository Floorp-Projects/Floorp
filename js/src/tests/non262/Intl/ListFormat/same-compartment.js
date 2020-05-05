// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.wrapWithProto)

var locale = "en";
var list = ["a", "b", "c"];

var listFormat = new Intl.ListFormat(locale);
var scwListFormat = wrapWithProto(listFormat, Intl.ListFormat.prototype);

// Intl.ListFormat.prototype.format
{
    var fn = Intl.ListFormat.prototype.format;

    var expectedValue = fn.call(listFormat, list);
    var actualValue = fn.call(scwListFormat, list);

    assertEq(actualValue, expectedValue);
}

// Intl.ListFormat.prototype.formatToParts
{
    var fn = Intl.ListFormat.prototype.formatToParts;

    var expectedValue = fn.call(listFormat, list);
    var actualValue = fn.call(scwListFormat, list);

    assertDeepEq(actualValue, expectedValue);
}

// Intl.ListFormat.prototype.resolvedOptions
{
    var fn = Intl.ListFormat.prototype.resolvedOptions;

    var expectedValue = fn.call(listFormat);
    var actualValue = fn.call(scwListFormat);

    assertDeepEq(actualValue, expectedValue);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
