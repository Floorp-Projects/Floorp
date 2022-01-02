// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Nan, Inf, Integer, MinusSign, PlusSign, Decimal, Fraction, Group,
    ExponentSeparator, ExponentInteger, ExponentMinusSign,
} = NumberFormatParts;

const testcases = [
    {
        locale: "en",
        options: {
            notation: "scientific",
        },
        values: [
            {value: +0, string: "0E0", parts: [Integer("0"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: -0, string: "-0E0", parts: [MinusSign("-"), Integer("0"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 0n, string: "0E0", parts: [Integer("0"), ExponentSeparator("E"), ExponentInteger("0")]},

            {value: 1, string: "1E0", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 10, string: "1E1", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("1")]},
            {value: 100, string: "1E2", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("2")]},
            {value: 1000, string: "1E3", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 10000, string: "1E4", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("4")]},
            {value: 100000, string: "1E5", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("5")]},
            {value: 1000000, string: "1E6", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("6")]},

            {value: 1n, string: "1E0", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 10n, string: "1E1", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("1")]},
            {value: 100n, string: "1E2", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("2")]},
            {value: 1000n, string: "1E3", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 10000n, string: "1E4", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("4")]},
            {value: 100000n, string: "1E5", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("5")]},
            {value: 1000000n, string: "1E6", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("6")]},

            {value: 0.1, string: "1E-1", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("1")]},
            {value: 0.01, string: "1E-2", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("2")]},
            {value: 0.001, string: "1E-3", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")]},
            {value: 0.0001, string: "1E-4", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("4")]},
            {value: 0.00001, string: "1E-5", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("5")]},
            {value: 0.000001, string: "1E-6", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("6")]},
            {value: 0.0000001, string: "1E-7", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("7")]},

            {value:  Infinity, string: "∞", parts: [Inf("∞")]},
            {value: -Infinity, string: "-∞", parts: [MinusSign("-"), Inf("∞")]},

            {value:  NaN, string: "NaN", parts: [Nan("NaN")]},
            {value: -NaN, string: "NaN", parts: [Nan("NaN")]},
        ],
    },

    // Exponent modifications take place early, so while in the "standard" notation
    // `Intl.NumberFormat("en", {maximumFractionDigits: 0}).format(0.1)` returns "0", for
    // "scientific" notation the result string is not "0", but instead "1E-1".

    {
        locale: "en",
        options: {
            notation: "scientific",
            maximumFractionDigits: 0,
        },
        values: [
            {value: 0.1, string: "1E-1", parts: [
                Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("1")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "scientific",
            minimumFractionDigits: 4,
        },
        values: [
            {value: 10, string: "1.0000E1", parts: [
                Integer("1"), Decimal("."), Fraction("0000"), ExponentSeparator("E"), ExponentInteger("1")
            ]},
            {value: 0.1, string: "1.0000E-1", parts: [
                Integer("1"), Decimal("."), Fraction("0000"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("1")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "scientific",
            minimumIntegerDigits: 4,
        },
        values: [
            {value: 10, string: "0,001E1", parts: [
                Integer("0"), Group(","), Integer("001"), ExponentSeparator("E"), ExponentInteger("1")
            ]},
            {value: 0.1, string: "0,001E-1", parts: [
                Integer("0"), Group(","), Integer("001"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("1")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "scientific",
            minimumSignificantDigits: 4,
        },
        values: [
            {value: 10, string: "1.000E1", parts: [
                Integer("1"), Decimal("."), Fraction("000"), ExponentSeparator("E"), ExponentInteger("1")
            ]},
            {value: 0.1, string: "1.000E-1", parts: [
                Integer("1"), Decimal("."), Fraction("000"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("1")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "scientific",
            maximumSignificantDigits: 1,
        },
        values: [
            {value: 12, string: "1E1", parts: [
                Integer("1"), ExponentSeparator("E"), ExponentInteger("1")
            ]},
            {value: 0.12, string: "1E-1", parts: [
                Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("1")
            ]},
        ],
    },
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
