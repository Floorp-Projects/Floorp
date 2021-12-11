// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Nan, Inf, Integer, MinusSign, PlusSign, Decimal, Fraction, Group,
    ExponentSeparator, ExponentInteger, ExponentMinusSign,
} = NumberFormatParts;

const testcases = [
    {
        locale: "en",
        options: {
            notation: "engineering",
        },
        values: [
            {value: +0, string: "0E0", parts: [Integer("0"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: -0, string: "-0E0", parts: [MinusSign("-"), Integer("0"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 0n, string: "0E0", parts: [Integer("0"), ExponentSeparator("E"), ExponentInteger("0")]},

            {value: 1, string: "1E0", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 10, string: "10E0", parts: [Integer("10"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 100, string: "100E0", parts: [Integer("100"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 1000, string: "1E3", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 10000, string: "10E3", parts: [Integer("10"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 100000, string: "100E3", parts: [Integer("100"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 1000000, string: "1E6", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("6")]},

            {value: 1n, string: "1E0", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 10n, string: "10E0", parts: [Integer("10"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 100n, string: "100E0", parts: [Integer("100"), ExponentSeparator("E"), ExponentInteger("0")]},
            {value: 1000n, string: "1E3", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 10000n, string: "10E3", parts: [Integer("10"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 100000n, string: "100E3", parts: [Integer("100"), ExponentSeparator("E"), ExponentInteger("3")]},
            {value: 1000000n, string: "1E6", parts: [Integer("1"), ExponentSeparator("E"), ExponentInteger("6")]},

            {value: 0.1, string: "100E-3", parts: [Integer("100"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")]},
            {value: 0.01, string: "10E-3", parts: [Integer("10"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")]},
            {value: 0.001, string: "1E-3", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")]},
            {value: 0.0001, string: "100E-6", parts: [Integer("100"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("6")]},
            {value: 0.00001, string: "10E-6", parts: [Integer("10"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("6")]},
            {value: 0.000001, string: "1E-6", parts: [Integer("1"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("6")]},
            {value: 0.0000001, string: "100E-9", parts: [Integer("100"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("9")]},

            {value:  Infinity, string: "∞", parts: [Inf("∞")]},
            {value: -Infinity, string: "-∞", parts: [MinusSign("-"), Inf("∞")]},

            {value:  NaN, string: "NaN", parts: [Nan("NaN")]},
            {value: -NaN, string: "NaN", parts: [Nan("NaN")]},
        ],
    },

    // Exponent modifications take place early, so while in the "standard" notation
    // `Intl.NumberFormat("en", {maximumFractionDigits: 0}).format(0.1)` returns "0", for
    // "engineering" notation the result string is not "0", but instead "100E-3".

    {
        locale: "en",
        options: {
            notation: "engineering",
            maximumFractionDigits: 0,
        },
        values: [
            {value: 0.1, string: "100E-3", parts: [
                Integer("100"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "engineering",
            minimumFractionDigits: 4,
        },
        values: [
            {value: 10, string: "10.0000E0", parts: [
                Integer("10"), Decimal("."), Fraction("0000"), ExponentSeparator("E"), ExponentInteger("0")
            ]},
            {value: 0.1, string: "100.0000E-3", parts: [
                Integer("100"), Decimal("."), Fraction("0000"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "engineering",
            minimumIntegerDigits: 4,
        },
        values: [
            {value: 10, string: "0,010E0", parts: [
                Integer("0"), Group(","), Integer("010"), ExponentSeparator("E"), ExponentInteger("0")
            ]},
            {value: 0.1, string: "0,100E-3", parts: [
                Integer("0"), Group(","), Integer("100"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "engineering",
            minimumSignificantDigits: 4,
        },
        values: [
            {value: 10, string: "10.00E0", parts: [
                Integer("10"), Decimal("."), Fraction("00"), ExponentSeparator("E"), ExponentInteger("0")
            ]},
            {value: 0.1, string: "100.0E-3", parts: [
                Integer("100"), Decimal("."), Fraction("0"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")
            ]},
        ],
    },

    {
        locale: "en",
        options: {
            notation: "engineering",
            maximumSignificantDigits: 1,
        },
        values: [
            {value: 12, string: "10E0", parts: [
                Integer("10"), ExponentSeparator("E"), ExponentInteger("0")
            ]},
            {value: 0.12, string: "100E-3", parts: [
                Integer("100"), ExponentSeparator("E"), ExponentMinusSign("-"), ExponentInteger("3")
            ]},
        ],
    },
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
