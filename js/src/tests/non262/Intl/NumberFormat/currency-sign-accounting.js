// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Nan, Inf, Integer, MinusSign, PlusSign, Decimal, Fraction,
    Currency, Literal,
} = NumberFormatParts;

const testcases = [
    // "auto": Show the sign on negative numbers only.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "auto",
        },
        values: [
            {value: +0, string: "$0.00",
             parts: [Currency("$"), Integer("0"), Decimal("."), Fraction("00")]},
            {value: -0, string: "($0.00)",
             parts: [Literal("("), Currency("$"), Integer("0"), Decimal("."), Fraction("00"), Literal(")")]},

            {value:  1, string: "$1.00",
             parts: [Currency("$"), Integer("1"), Decimal("."), Fraction("00")]},
            {value: -1, string: "($1.00)",
             parts: [Literal("("), Currency("$"), Integer("1"), Decimal("."), Fraction("00"), Literal(")")]},

            {value:  Infinity, string: "$∞", parts: [Currency("$"), Inf("∞")]},
            {value: -Infinity, string: "($∞)", parts: [Literal("("), Currency("$"), Inf("∞"), Literal(")")]},

            {value:  NaN, string: "$NaN", parts: [Currency("$"), Nan("NaN")]},
            {value: -NaN, string: "$NaN", parts: [Currency("$"), Nan("NaN")]},
        ],
    },

    // "never": Show the sign on neither positive nor negative numbers.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "never",
        },
        values: [
            {value: +0, string: "$0.00", parts: [Currency("$"), Integer("0"), Decimal("."), Fraction("00")]},
            {value: -0, string: "$0.00", parts: [Currency("$"), Integer("0"), Decimal("."), Fraction("00")]},

            {value:  1, string: "$1.00", parts: [Currency("$"), Integer("1"), Decimal("."), Fraction("00")]},
            {value: -1, string: "$1.00", parts: [Currency("$"), Integer("1"), Decimal("."), Fraction("00")]},

            {value:  Infinity, string: "$∞", parts: [Currency("$"), Inf("∞")]},
            {value: -Infinity, string: "$∞", parts: [Currency("$"), Inf("∞")]},

            {value:  NaN, string: "$NaN", parts: [Currency("$"), Nan("NaN")]},
            {value: -NaN, string: "$NaN", parts: [Currency("$"), Nan("NaN")]},
        ],
    },

    // "always": Show the sign on positive and negative numbers including zero.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "always",
        },
        values: [
            {value: +0, string: "+$0.00",
             parts: [PlusSign("+"), Currency("$"), Integer("0"), Decimal("."), Fraction("00")]},
            {value: -0, string: "($0.00)",
             parts: [Literal("("), Currency("$"), Integer("0"), Decimal("."), Fraction("00"), Literal(")")]},

            {value:  1, string: "+$1.00",
             parts: [PlusSign("+"), Currency("$"), Integer("1"), Decimal("."), Fraction("00")]},
            {value: -1, string: "($1.00)",
             parts: [Literal("("), Currency("$"), Integer("1"), Decimal("."), Fraction("00"), Literal(")")]},

            {value:  Infinity, string: "+$∞", parts: [PlusSign("+"), Currency("$"), Inf("∞")]},
            {value: -Infinity, string: "($∞)", parts: [Literal("("), Currency("$"), Inf("∞"), Literal(")")]},

            {value:  NaN, string: "+$NaN", parts: [PlusSign("+"), Currency("$"), Nan("NaN")]},
            {value: -NaN, string: "+$NaN", parts: [PlusSign("+"), Currency("$"), Nan("NaN")]},
        ],
    },

    // "exceptZero": Show the sign on positive and negative numbers but not zero.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "exceptZero",
        },
        values: [
            {value: +0, string: "$0.00",
             parts: [Currency("$"), Integer("0"), Decimal("."), Fraction("00")]},
            {value: -0, string: "$0.00",
             parts: [Currency("$"), Integer("0"), Decimal("."), Fraction("00")]},

            {value:  1, string: "+$1.00",
             parts: [PlusSign("+"), Currency("$"), Integer("1"), Decimal("."), Fraction("00")]},
            {value: -1, string: "($1.00)",
             parts: [Literal("("), Currency("$"), Integer("1"), Decimal("."), Fraction("00"), Literal(")")]},

            {value:  Infinity, string: "+$∞", parts: [PlusSign("+"), Currency("$"), Inf("∞")]},
            {value: -Infinity, string: "($∞)", parts: [Literal("("), Currency("$"), Inf("∞"), Literal(")")]},

            {value:  NaN, string: "$NaN", parts: [Currency("$"), Nan("NaN")]},
            {value: -NaN, string: "$NaN", parts: [Currency("$"), Nan("NaN")]},
        ],
    },

    // Tests with suppressed fractional digits.

    // "auto": Show the sign on negative numbers only.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "auto",
            minimumFractionDigits: 0,
            maximumFractionDigits: 0,
        },
        values: [
            {value: +0.1, string: "$0", parts: [Currency("$"), Integer("0")]},
            {value: -0.1, string: "($0)", parts: [Literal("("), Currency("$"), Integer("0"), Literal(")")]},
        ],
    },

    // "never": Show the sign on neither positive nor negative numbers.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "never",
            minimumFractionDigits: 0,
            maximumFractionDigits: 0,
        },
        values: [
            {value: +0.1, string: "$0", parts: [Currency("$"), Integer("0")]},
            {value: -0.1, string: "$0", parts: [Currency("$"), Integer("0")]},
        ],
    },

    // "always": Show the sign on positive and negative numbers including zero.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "always",
            minimumFractionDigits: 0,
            maximumFractionDigits: 0,
        },
        values: [
            {value: +0.1, string: "+$0", parts: [PlusSign("+"), Currency("$"), Integer("0")]},
            {value: -0.1, string: "($0)", parts: [Literal("("), Currency("$"), Integer("0"), Literal(")")]},
        ],
    },

    // "exceptZero": Show the sign on positive and negative numbers but not zero.
    {
        locale: "en",
        options: {
            style: "currency",
            currency: "USD",
            currencySign: "accounting",
            signDisplay: "exceptZero",
            minimumFractionDigits: 0,
            maximumFractionDigits: 0,
        },

        values: [
            {value: +0.1, string: "$0", parts: [Currency("$"), Integer("0")]},
            {value: -0.1, string: "$0", parts: [Currency("$"), Integer("0")]},
        ],
    }
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
