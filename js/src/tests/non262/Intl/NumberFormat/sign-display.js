// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Nan, Inf, Integer, MinusSign, PlusSign, Decimal, Fraction
} = NumberFormatParts;

const testcases = [
    // "auto": Show the sign on negative numbers only.
    {
        locale: "en",
        options: {
            signDisplay: "auto",
        },
        values: [
            {value: +0, string: "0", parts: [Integer("0")]},
            {value: -0, string: "-0", parts: [MinusSign("-"), Integer("0")]},
            {value: 0n, string: "0", parts: [Integer("0")]},

            {value:   1, string: "1", parts: [Integer("1")]},
            {value:  -1, string: "-1", parts: [MinusSign("-"), Integer("1")]},
            {value:  1n, string: "1", parts: [Integer("1")]},
            {value: -1n, string: "-1", parts: [MinusSign("-"), Integer("1")]},

            {value: 0.1, string: "0.1", parts: [Integer("0"), Decimal("."), Fraction("1")]},
            {value: -0.1, string: "-0.1", parts: [MinusSign("-"), Integer("0"), Decimal("."), Fraction("1")]},

            {value: 0.9, string: "0.9", parts: [Integer("0"), Decimal("."), Fraction("9")]},
            {value: -0.9, string: "-0.9", parts: [MinusSign("-"), Integer("0"), Decimal("."), Fraction("9")]},

            {value:  Infinity, string: "∞", parts: [Inf("∞")]},
            {value: -Infinity, string: "-∞", parts: [MinusSign("-"), Inf("∞")]},

            {value:  NaN, string: "NaN", parts: [Nan("NaN")]},
            {value: -NaN, string: "NaN", parts: [Nan("NaN")]},
        ],
    },

    // "never": Show the sign on neither positive nor negative numbers.
    {
        locale: "en",
        options: {
            signDisplay: "never",
        },
        values: [
            {value: +0, string: "0", parts: [Integer("0")]},
            {value: -0, string: "0", parts: [Integer("0")]},
            {value: 0n, string: "0", parts: [Integer("0")]},

            {value:   1, string: "1", parts: [Integer("1")]},
            {value:  -1, string: "1", parts: [Integer("1")]},
            {value:  1n, string: "1", parts: [Integer("1")]},
            {value: -1n, string: "1", parts: [Integer("1")]},

            {value: 0.1, string: "0.1", parts: [Integer("0"), Decimal("."), Fraction("1")]},
            {value: -0.1, string: "0.1", parts: [Integer("0"), Decimal("."), Fraction("1")]},

            {value: 0.9, string: "0.9", parts: [Integer("0"), Decimal("."), Fraction("9")]},
            {value: -0.9, string: "0.9", parts: [Integer("0"), Decimal("."), Fraction("9")]},

            {value:  Infinity, string: "∞", parts: [Inf("∞")]},
            {value: -Infinity, string: "∞", parts: [Inf("∞")]},

            {value:  NaN, string: "NaN", parts: [Nan("NaN")]},
            {value: -NaN, string: "NaN", parts: [Nan("NaN")]},
        ],
    },

    // "always": Show the sign on positive and negative numbers including zero.
    {
        locale: "en",
        options: {
            signDisplay: "always",
        },
        values: [
            {value: +0, string: "+0", parts: [PlusSign("+"), Integer("0")]},
            {value: -0, string: "-0", parts: [MinusSign("-"), Integer("0")]},
            {value: 0n, string: "+0", parts: [PlusSign("+"), Integer("0")]},

            {value:   1, string: "+1", parts: [PlusSign("+"), Integer("1")]},
            {value:  -1, string: "-1", parts: [MinusSign("-"), Integer("1")]},
            {value:  1n, string: "+1", parts: [PlusSign("+"), Integer("1")]},
            {value: -1n, string: "-1", parts: [MinusSign("-"), Integer("1")]},

            {value: 0.1, string: "+0.1", parts: [PlusSign("+"), Integer("0"), Decimal("."), Fraction("1")]},
            {value: -0.1, string: "-0.1", parts: [MinusSign("-"), Integer("0"), Decimal("."), Fraction("1")]},

            {value: 0.9, string: "+0.9", parts: [PlusSign("+"), Integer("0"), Decimal("."), Fraction("9")]},
            {value: -0.9, string: "-0.9", parts: [MinusSign("-"), Integer("0"), Decimal("."), Fraction("9")]},

            {value:  Infinity, string: "+∞", parts: [PlusSign("+"), Inf("∞")]},
            {value: -Infinity, string: "-∞", parts: [MinusSign("-"), Inf("∞")]},

            {value:  NaN, string: "+NaN", parts: [PlusSign("+"), Nan("NaN")]},
            {value: -NaN, string: "+NaN", parts: [PlusSign("+"), Nan("NaN")]},
        ],
    },

    // "exceptZero": Show the sign on positive and negative numbers but not zero.
    {
        locale: "en",
        options: {
            signDisplay: "exceptZero",
        },
        values: [
            {value: +0, string: "0", parts: [Integer("0")]},
            {value: -0, string: "0", parts: [Integer("0")]},
            {value: 0n, string: "0", parts: [Integer("0")]},

            {value:   1, string: "+1", parts: [PlusSign("+"), Integer("1")]},
            {value:  -1, string: "-1", parts: [MinusSign("-"), Integer("1")]},
            {value:  1n, string: "+1", parts: [PlusSign("+"), Integer("1")]},
            {value: -1n, string: "-1", parts: [MinusSign("-"), Integer("1")]},

            {value: 0.1, string: "+0.1", parts: [PlusSign("+"), Integer("0"), Decimal("."), Fraction("1")]},
            {value: -0.1, string: "-0.1", parts: [MinusSign("-"), Integer("0"), Decimal("."), Fraction("1")]},

            {value: 0.9, string: "+0.9", parts: [PlusSign("+"), Integer("0"), Decimal("."), Fraction("9")]},
            {value: -0.9, string: "-0.9", parts: [MinusSign("-"), Integer("0"), Decimal("."), Fraction("9")]},

            {value:  Infinity, string: "+∞", parts: [PlusSign("+"), Inf("∞")]},
            {value: -Infinity, string: "-∞", parts: [MinusSign("-"), Inf("∞")]},

            {value:  NaN, string: "NaN", parts: [Nan("NaN")]},
            {value: -NaN, string: "NaN", parts: [Nan("NaN")]},
        ],
    },

    // Tests with suppressed fractional digits.

    // "auto": Show the sign on negative numbers only.
    {
        locale: "en",
        options: {
            signDisplay: "auto",
            maximumFractionDigits: 0,
        },
        values: [
            {value: +0.1, string: "0", parts: [Integer("0")]},
            {value: -0.1, string: "-0", parts: [MinusSign("-"), Integer("0")]},
        ],
    },

    // "never": Show the sign on neither positive nor negative numbers.
    {
        locale: "en",
        options: {
            signDisplay: "never",
            maximumFractionDigits: 0,
        },
        values: [
            {value: +0.1, string: "0", parts: [Integer("0")]},
            {value: -0.1, string: "0", parts: [Integer("0")]},
        ],
    },

    // "always": Show the sign on positive and negative numbers including zero.
    {
        locale: "en",
        options: {
            signDisplay: "always",
            maximumFractionDigits: 0,
        },
        values: [
            {value: +0.1, string: "+0", parts: [PlusSign("+"), Integer("0")]},
            {value: -0.1, string: "-0", parts: [MinusSign("-"), Integer("0")]},
        ],
    },

    // "exceptZero": Show the sign on positive and negative numbers but not zero.
    {
        locale: "en",
        options: {
            signDisplay: "exceptZero",
            maximumFractionDigits: 0,
        },

        values: [
            {value: +0.1, string: "0", parts: [Integer("0")]},
            {value: -0.1, string: "0", parts: [Integer("0")]},
        ],
    }
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
