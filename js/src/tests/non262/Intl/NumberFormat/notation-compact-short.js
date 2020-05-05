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
            compactDisplay: "short",
        },
        values: [
            {value: +0, string: "0", parts: [Integer("0")]},
            {value: -0, string: "-0", parts: [MinusSign("-"), Integer("0")]},
            {value: 0n, string: "0", parts: [Integer("0")]},

            {value: 1, string: "1", parts: [Integer("1")]},
            {value: 10, string: "10", parts: [Integer("10")]},
            {value: 100, string: "100", parts: [Integer("100")]},
            {value: 1000, string: "1K", parts: [Integer("1"), Compact("K")]},
            {value: 10000, string: "10K", parts: [Integer("10"), Compact("K")]},
            {value: 100000, string: "100K", parts: [Integer("100"), Compact("K")]},
            {value: 1000000, string: "1M", parts: [Integer("1"), Compact("M")]},
            {value: 10000000, string: "10M", parts: [Integer("10"), Compact("M")]},
            {value: 100000000, string: "100M", parts: [Integer("100"), Compact("M")]},
            {value: 1000000000, string: "1B", parts: [Integer("1"), Compact("B")]},
            {value: 10000000000, string: "10B", parts: [Integer("10"), Compact("B")]},
            {value: 100000000000, string: "100B", parts: [Integer("100"), Compact("B")]},
            {value: 1000000000000, string: "1T", parts: [Integer("1"), Compact("T")]},
            {value: 10000000000000, string: "10T", parts: [Integer("10"), Compact("T")]},
            {value: 100000000000000, string: "100T", parts: [Integer("100"), Compact("T")]},
            {value: 1000000000000000, string: "1000T", parts: [Integer("1000"), Compact("T")]},
            {value: 10000000000000000, string: "10,000T", parts: [Integer("10"), Group(","), Integer("000"), Compact("T")]},
            {value: 100000000000000000, string: "100,000T", parts: [Integer("100"), Group(","), Integer("000"), Compact("T")]},

            {value: 1n, string: "1", parts: [Integer("1")]},
            {value: 10n, string: "10", parts: [Integer("10")]},
            {value: 100n, string: "100", parts: [Integer("100")]},
            {value: 1000n, string: "1K", parts: [Integer("1"), Compact("K")]},
            {value: 10000n, string: "10K", parts: [Integer("10"), Compact("K")]},
            {value: 100000n, string: "100K", parts: [Integer("100"), Compact("K")]},
            {value: 1000000n, string: "1M", parts: [Integer("1"), Compact("M")]},
            {value: 10000000n, string: "10M", parts: [Integer("10"), Compact("M")]},
            {value: 100000000n, string: "100M", parts: [Integer("100"), Compact("M")]},
            {value: 1000000000n, string: "1B", parts: [Integer("1"), Compact("B")]},
            {value: 10000000000n, string: "10B", parts: [Integer("10"), Compact("B")]},
            {value: 100000000000n, string: "100B", parts: [Integer("100"), Compact("B")]},
            {value: 1000000000000n, string: "1T", parts: [Integer("1"), Compact("T")]},
            {value: 10000000000000n, string: "10T", parts: [Integer("10"), Compact("T")]},
            {value: 100000000000000n, string: "100T", parts: [Integer("100"), Compact("T")]},
            {value: 1000000000000000n, string: "1000T", parts: [Integer("1000"), Compact("T")]},
            {value: 10000000000000000n, string: "10,000T", parts: [Integer("10"), Group(","), Integer("000"), Compact("T")]},
            {value: 100000000000000000n, string: "100,000T", parts: [Integer("100"), Group(","), Integer("000"), Compact("T")]},

            {value: 0.1, string: "0.1", parts: [Integer("0"), Decimal("."), Fraction("1")]},
            {value: 0.01, string: "0.01", parts: [Integer("0"), Decimal("."), Fraction("01")]},
            {value: 0.001, string: "0.001", parts: [Integer("0"), Decimal("."), Fraction("001")]},
            {value: 0.0001, string: "0.0001", parts: [Integer("0"), Decimal("."), Fraction("0001")]},
            {value: 0.00001, string: "0.00001", parts: [Integer("0"), Decimal("."), Fraction("00001")]},
            {value: 0.000001, string: "0.000001", parts: [Integer("0"), Decimal("."), Fraction("000001")]},
            {value: 0.0000001, string: "0.0000001", parts: [Integer("0"), Decimal("."), Fraction("0000001")]},

            {value: 12, string: "12", parts: [Integer("12")]},
            {value: 123, string: "123", parts: [Integer("123")]},
            {value: 1234, string: "1.2K", parts: [Integer("1"), Decimal("."), Fraction("2"), Compact("K")]},
            {value: 12345, string: "12K", parts: [Integer("12"), Compact("K")]},
            {value: 123456, string: "123K", parts: [Integer("123"), Compact("K")]},
            {value: 1234567, string: "1.2M", parts: [Integer("1"), Decimal("."), Fraction("2"), Compact("M")]},
            {value: 12345678, string: "12M", parts: [Integer("12"), Compact("M")]},
            {value: 123456789, string: "123M", parts: [Integer("123"), Compact("M")]},

            {value:  Infinity, string: "∞", parts: [Inf("∞")]},
            {value: -Infinity, string: "-∞", parts: [MinusSign("-"), Inf("∞")]},

            {value:  NaN, string: "NaN", parts: [Nan("NaN")]},
            {value: -NaN, string: "NaN", parts: [Nan("NaN")]},
        ],
    },

    // Compact symbol can consist of multiple characters, e.g. when it's an abbreviation.
    {
        locale: "de",
        options: {
            notation: "compact",
            compactDisplay: "short",
        },
        values: [
            {value: 1e6, string: "1\u00A0Mio.", parts: [Integer("1"), Literal("\u00A0"), Compact("Mio.")]},
            {value: 1e9, string: "1\u00A0Mrd.", parts: [Integer("1"), Literal("\u00A0"), Compact("Mrd.")]},
            {value: 1e12, string: "1\u00A0Bio.", parts: [Integer("1"), Literal("\u00A0"), Compact("Bio.")]},

            // CLDR doesn't support "Billiarde" (Brd.), Trillion (Trill.), etc.
            {value: 1e15, string: "1000\u00A0Bio.", parts: [Integer("1000"), Literal("\u00A0"), Compact("Bio.")]},
        ],
    },

    // Digits are grouped in myriads (every 10,000) in Chinese.
    {
        locale: "zh",
        options: {
            notation: "compact",
            compactDisplay: "short",
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

            {value: 10e7, string: "1\u4EBF", parts: [Integer("1"), Compact("\u4EBF")]},
            {value: 100e7, string: "10\u4EBF", parts: [Integer("10"), Compact("\u4EBF")]},
            {value: 1000e7, string: "100\u4EBF", parts: [Integer("100"), Compact("\u4EBF")]},
            {value: 10000e7, string: "1000\u4EBF", parts: [Integer("1000"), Compact("\u4EBF")]},

            {value: 10e11, string: "1\u4E07\u4EBF", parts: [Integer("1"), Compact("\u4E07\u4EBF")]},
            {value: 100e11, string: "10\u4E07\u4EBF", parts: [Integer("10"), Compact("\u4E07\u4EBF")]},
            {value: 1000e11, string: "100\u4E07\u4EBF", parts: [Integer("100"), Compact("\u4E07\u4EBF")]},
            {value: 10000e11, string: "1000\u4E07\u4EBF", parts: [Integer("1000"), Compact("\u4E07\u4EBF")]},

            {value: 100000e11, string: "10,000\u4E07\u4EBF", parts: [Integer("10"), Group(","), Integer("000"), Compact("\u4E07\u4EBF")]},
        ],
    },
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
