// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Integer, Decimal, Fraction, Currency, Literal,
} = NumberFormatParts;

const testcases = [
    {
        locale: "en-CA",
        options: {
            style: "currency",
            currency: "USD",
            currencyDisplay: "narrowSymbol",
        },
        values: [
            {value: 123, string: "$123.00",
             parts: [Currency("$"), Integer("123"), Decimal("."), Fraction("00")]},
        ],
    },

    // And for comparison "symbol" currency-display.

    {
        locale: "en-CA",
        options: {
            style: "currency",
            currency: "USD",
            currencyDisplay: "symbol",
        },
        values: [
            {value: 123, string: "US$123.00",
             parts: [Currency("US$"), Integer("123"), Decimal("."), Fraction("00")]},
        ],
    },
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
