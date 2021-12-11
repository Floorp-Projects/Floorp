// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const {
    Nan, Inf, Integer, MinusSign, PlusSign, Literal,
    Unit
} = NumberFormatParts;

const testcases = [
    {
        locale: "en",
        options: {
            style: "unit",
            unit: "meter",
            unitDisplay: "short",
        },
        values: [
            {value: +0, string: "0 m", parts: [Integer("0"), Literal(" "), Unit("m")]},
            {value: -0, string: "-0 m", parts: [MinusSign("-"), Integer("0"), Literal(" "), Unit("m")]},
            {value: 0n, string: "0 m", parts: [Integer("0"), Literal(" "), Unit("m")]},

            {value:   1, string: "1 m", parts: [Integer("1"), Literal(" "), Unit("m")]},
            {value:  -1, string: "-1 m", parts: [MinusSign("-"), Integer("1"), Literal(" "), Unit("m")]},
            {value:  1n, string: "1 m", parts: [Integer("1"), Literal(" "), Unit("m")]},
            {value: -1n, string: "-1 m", parts: [MinusSign("-"), Integer("1"), Literal(" "), Unit("m")]},

            {value:  Infinity, string: "∞ m", parts: [Inf("∞"), Literal(" "), Unit("m")]},
            {value: -Infinity, string: "-∞ m", parts: [MinusSign("-"), Inf("∞"), Literal(" "), Unit("m")]},

            {value:  NaN, string: "NaN m", parts: [Nan("NaN"), Literal(" "), Unit("m")]},
            {value: -NaN, string: "NaN m", parts: [Nan("NaN"), Literal(" "), Unit("m")]},
        ],
    },

    {
        locale: "en",
        options: {
            style: "unit",
            unit: "meter",
            unitDisplay: "narrow",
        },
        values: [
            {value: +0, string: "0m", parts: [Integer("0"), Unit("m")]},
            {value: -0, string: "-0m", parts: [MinusSign("-"), Integer("0"), Unit("m")]},
            {value: 0n, string: "0m", parts: [Integer("0"), Unit("m")]},

            {value:   1, string: "1m", parts: [Integer("1"), Unit("m")]},
            {value:  -1, string: "-1m", parts: [MinusSign("-"), Integer("1"), Unit("m")]},
            {value:  1n, string: "1m", parts: [Integer("1"), Unit("m")]},
            {value: -1n, string: "-1m", parts: [MinusSign("-"), Integer("1"), Unit("m")]},

            {value:  Infinity, string: "∞m", parts: [Inf("∞"), Unit("m")]},
            {value: -Infinity, string: "-∞m", parts: [MinusSign("-"), Inf("∞"), Unit("m")]},

            {value:  NaN, string: "NaNm", parts: [Nan("NaN"), Unit("m")]},
            {value: -NaN, string: "NaNm", parts: [Nan("NaN"), Unit("m")]},
        ],
    },

    {
        locale: "en",
        options: {
            style: "unit",
            unit: "meter",
            unitDisplay: "long",
        },
        values: [
            {value: +0, string: "0 meters", parts: [Integer("0"), Literal(" "), Unit("meters")]},
            {value: -0, string: "-0 meters", parts: [MinusSign("-"), Integer("0"), Literal(" "), Unit("meters")]},
            {value: 0n, string: "0 meters", parts: [Integer("0"), Literal(" "), Unit("meters")]},

            {value:   1, string: "1 meter", parts: [Integer("1"), Literal(" "), Unit("meter")]},
            {value:  -1, string: "-1 meter", parts: [MinusSign("-"), Integer("1"), Literal(" "), Unit("meter")]},
            {value:  1n, string: "1 meter", parts: [Integer("1"), Literal(" "), Unit("meter")]},
            {value: -1n, string: "-1 meter", parts: [MinusSign("-"), Integer("1"), Literal(" "), Unit("meter")]},

            {value:  Infinity, string: "∞ meters", parts: [Inf("∞"), Literal(" "), Unit("meters")]},
            {value: -Infinity, string: "-∞ meters", parts: [MinusSign("-"), Inf("∞"), Literal(" "), Unit("meters")]},

            {value:  NaN, string: "NaN meters", parts: [Nan("NaN"), Literal(" "), Unit("meters")]},
            {value: -NaN, string: "NaN meters", parts: [Nan("NaN"), Literal(" "), Unit("meters")]},
        ],
    },

    // Ensure the correct compound unit is automatically selected by ICU. For
    // example instead of "50 chilometri al orari", 50 km/h should return
    // "50 chilometri orari" in Italian.

    {
        locale: "it",
        options: {
            style: "unit",
            unit: "kilometer-per-hour",
            unitDisplay: "long",
        },
        values: [
            {value: 50, string: "50 chilometri orari", parts: [Integer("50"), Literal(" "), Unit("chilometri orari")]},
        ],
    },
];

runNumberFormattingTestcases(testcases);

if (typeof reportCompare === "function")
    reportCompare(true, true);
