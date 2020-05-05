// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Nan, Inf, Integer, MinusSign, PlusSign, Decimal, Fraction, Group, Literal,
    Compact,
} = NumberFormatParts;

const testcases = [
    {
        locale: "en",
        options: {
            notation: "compact",
            compactDisplay: "long",
        },
        values: [
            {value: +0, string: "0", parts: [Integer("0")]},
            {value: -0, string: "-0", parts: [MinusSign("-"), Integer("0")]},
            {value: 0n, string: "0", parts: [Integer("0")]},

            {value: 1, string: "1", parts: [Integer("1")]},
            {value: 10, string: "10", parts: [Integer("10")]},
            {value: 100, string: "100", parts: [Integer("100")]},
            {value: 1000, string: "1 thousand", parts: [Integer("1"), Literal(" "), Compact("thousand")]},
            {value: 10000, string: "10 thousand", parts: [Integer("10"), Literal(" "), Compact("thousand")]},
            {value: 100000, string: "100 thousand", parts: [Integer("100"), Literal(" "), Compact("thousand")]},
            {value: 1000000, string: "1 million", parts: [Integer("1"), Literal(" "), Compact("million")]},
            {value: 10000000, string: "10 million", parts: [Integer("10"), Literal(" "), Compact("million")]},
            {value: 100000000, string: "100 million", parts: [Integer("100"), Literal(" "), Compact("million")]},
            {value: 1000000000, string: "1 billion", parts: [Integer("1"), Literal(" "), Compact("billion")]},
            {value: 10000000000, string: "10 billion", parts: [Integer("10"), Literal(" "), Compact("billion")]},
            {value: 100000000000, string: "100 billion", parts: [Integer("100"), Literal(" "), Compact("billion")]},
            {value: 1000000000000, string: "1 trillion", parts: [Integer("1"), Literal(" "), Compact("trillion")]},
            {value: 10000000000000, string: "10 trillion", parts: [Integer("10"), Literal(" "), Compact("trillion")]},
            {value: 100000000000000, string: "100 trillion", parts: [Integer("100"), Literal(" "), Compact("trillion")]},
            {value: 1000000000000000, string: "1000 trillion", parts: [Integer("1000"), Literal(" "), Compact("trillion")]},
            {value: 10000000000000000, string: "10,000 trillion", parts: [Integer("10"), Group(","), Integer("000"), Literal(" "), Compact("trillion")]},
            {value: 100000000000000000, string: "100,000 trillion", parts: [Integer("100"), Group(","), Integer("000"), Literal(" "), Compact("trillion")]},

            {value: 1n, string: "1", parts: [Integer("1")]},
            {value: 10n, string: "10", parts: [Integer("10")]},
            {value: 100n, string: "100", parts: [Integer("100")]},
            {value: 1000n, string: "1 thousand", parts: [Integer("1"), Literal(" "), Compact("thousand")]},
            {value: 10000n, string: "10 thousand", parts: [Integer("10"), Literal(" "), Compact("thousand")]},
            {value: 100000n, string: "100 thousand", parts: [Integer("100"), Literal(" "), Compact("thousand")]},
            {value: 1000000n, string: "1 million", parts: [Integer("1"), Literal(" "), Compact("million")]},
            {value: 10000000n, string: "10 million", parts: [Integer("10"), Literal(" "), Compact("million")]},
            {value: 100000000n, string: "100 million", parts: [Integer("100"), Literal(" "), Compact("million")]},
            {value: 1000000000n, string: "1 billion", parts: [Integer("1"), Literal(" "), Compact("billion")]},
            {value: 10000000000n, string: "10 billion", parts: [Integer("10"), Literal(" "), Compact("billion")]},
            {value: 100000000000n, string: "100 billion", parts: [Integer("100"), Literal(" "), Compact("billion")]},
            {value: 1000000000000n, string: "1 trillion", parts: [Integer("1"), Literal(" "), Compact("trillion")]},
            {value: 10000000000000n, string: "10 trillion", parts: [Integer("10"), Literal(" "), Compact("trillion")]},
            {value: 100000000000000n, string: "100 trillion", parts: [Integer("100"), Literal(" "), Compact("trillion")]},
            {value: 1000000000000000n, string: "1000 trillion", parts: [Integer("1000"), Literal(" "), Compact("trillion")]},
            {value: 10000000000000000n, string: "10,000 trillion", parts: [Integer("10"), Group(","), Integer("000"), Literal(" "), Compact("trillion")]},
            {value: 100000000000000000n, string: "100,000 trillion", parts: [Integer("100"), Group(","), Integer("000"), Literal(" "), Compact("trillion")]},

            {value: 0.1, string: "0.1", parts: [Integer("0"), Decimal("."), Fraction("1")]},
            {value: 0.01, string: "0.01", parts: [Integer("0"), Decimal("."), Fraction("01")]},
            {value: 0.001, string: "0.001", parts: [Integer("0"), Decimal("."), Fraction("001")]},
            {value: 0.0001, string: "0.0001", parts: [Integer("0"), Decimal("."), Fraction("0001")]},
            {value: 0.00001, string: "0.00001", parts: [Integer("0"), Decimal("."), Fraction("00001")]},
            {value: 0.000001, string: "0.000001", parts: [Integer("0"), Decimal("."), Fraction("000001")]},
            {value: 0.0000001, string: "0.0000001", parts: [Integer("0"), Decimal("."), Fraction("0000001")]},

            {value: 12, string: "12", parts: [Integer("12")]},
            {value: 123, string: "123", parts: [Integer("123")]},
            {value: 1234, string: "1.2 thousand", parts: [Integer("1"), Decimal("."), Fraction("2"), Literal(" "), Compact("thousand")]},
            {value: 12345, string: "12 thousand", parts: [Integer("12"), Literal(" "), Compact("thousand")]},
            {value: 123456, string: "123 thousand", parts: [Integer("123"), Literal(" "), Compact("thousand")]},
            {value: 1234567, string: "1.2 million", parts: [Integer("1"), Decimal("."), Fraction("2"), Literal(" "), Compact("million")]},
            {value: 12345678, string: "12 million", parts: [Integer("12"), Literal(" "), Compact("million")]},
            {value: 123456789, string: "123 million", parts: [Integer("123"), Literal(" "), Compact("million")]},

            {value:  Infinity, string: "∞", parts: [Inf("∞")]},
            {value: -Infinity, string: "-∞", parts: [MinusSign("-"), Inf("∞")]},

            {value:  NaN, string: "NaN", parts: [Nan("NaN")]},
            {value: -NaN, string: "NaN", parts: [Nan("NaN")]},
        ],
    },

    // The "{compactName}" placeholder may have different plural forms.
    {
        locale: "de",
        options: {
            notation: "compact",
            compactDisplay: "long",
        },
        values: [
            {value: 1e6, string: "1 Million", parts: [Integer("1"), Literal(" "), Compact("Million")]},
            {value: 2e6, string: "2 Millionen", parts: [Integer("2"), Literal(" "), Compact("Millionen")]},
            {value: 1e9, string: "1 Milliarde", parts: [Integer("1"), Literal(" "), Compact("Milliarde")]},
            {value: 2e9, string: "2 Milliarden", parts: [Integer("2"), Literal(" "), Compact("Milliarden")]},
        ],
    },

    // Digits are grouped in myriads (every 10,000) in Japanese.
    {
        locale: "ja",
        options: {
            notation: "compact",
            compactDisplay: "long",
        },
        values: [
            {value: 1, string: "1", parts: [Integer("1")]},
            {value: 10, string: "10", parts: [Integer("10")]},
            {value: 100, string: "100", parts: [Integer("100")]},
            {value: 1000, string: "1000", parts: [Integer("1000")]},

            {value: 10e3, string: "1\u4E07", parts: [Integer("1"), Compact("\u4E07")]},
            {value: 100e3, string: "10\u4E07", parts: [Integer("10"), Compact("\u4E07")]},
            {value: 1000e3, string: "100\u4E07", parts: [Integer("100"), Compact("\u4E07")]},
            {value: 10000e3, string: "1000\u4E07", parts: [Integer("1000"), Compact("\u4E07")]},

            {value: 10e7, string: "1\u5104", parts: [Integer("1"), Compact("\u5104")]},
            {value: 100e7, string: "10\u5104", parts: [Integer("10"), Compact("\u5104")]},
            {value: 1000e7, string: "100\u5104", parts: [Integer("100"), Compact("\u5104")]},
            {value: 10000e7, string: "1000\u5104", parts: [Integer("1000"), Compact("\u5104")]},

            {value: 10e11, string: "1\u5146", parts: [Integer("1"), Compact("\u5146")]},
            {value: 100e11, string: "10\u5146", parts: [Integer("10"), Compact("\u5146")]},
            {value: 1000e11, string: "100\u5146", parts: [Integer("100"), Compact("\u5146")]},
            {value: 10000e11, string: "1000\u5146", parts: [Integer("1000"), Compact("\u5146")]},

            {value: 100000e11, string: "10,000\u5146", parts: [Integer("10"), Group(","), Integer("000"), Compact("\u5146")]},
        ],
    },
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
