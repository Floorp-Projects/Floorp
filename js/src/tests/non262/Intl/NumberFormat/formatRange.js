// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const tests = {
  "en": {
    options: {},
    ranges: [
      // Values around zero.
      {start: 0, end: 0, result: "~0"},
      {start: -0, end: 0, result: "-0 – 0"},
      {start: -0, end: 0.1e-3, result: "-0 – 0"},
      {start: -0, end: "0.1e-3", result: "-0 – 0"},
      {start: "-0", end: 0.1e-3, result: "-0 – 0"},
      {start: -0, end: -0, result: "~-0"},

      // Values starting at negative infinity.
      {start: -Infinity, end: -Infinity, result: "~-∞"},
      {start: -Infinity, end: -0, result: "-∞ – -0"},
      {start: -Infinity, end: +0, result: "-∞ – 0"},
      {start: -Infinity, end: +Infinity, result: "-∞ – ∞"},

      // Values ending at positive infinity.
      {start: Infinity, end: Infinity, result: "~∞"},

      // Non-special cases.
      {start: 1, end: 100, result: "1–100"},
      {start: -100, end: 100, result: "-100 – 100"},
      {start: -1000, end: -100, result: "-1,000 – -100"},
      {start: Math.PI, end: 123_456.789, result: "3.142–123,456.789"},
      {start: -Math.PI, end: Math.E, result: "-3.142 – 2.718"},
      {
        start: Number.MAX_SAFE_INTEGER,
        end: 9007199254740993,
        result: "9,007,199,254,740,991–9,007,199,254,740,992",
      },
      {
        start: Number.MAX_SAFE_INTEGER,
        end: "9007199254740993",
        result: "9,007,199,254,740,991–9,007,199,254,740,993",
      },
    ],
  },
  "de": {
    options: {style: "currency", currency: "EUR"},
    ranges: [
      // Values around zero.
      {start: 0, end: 0, result: "≈0,00 €"},
      {start: -0, end: 0, result: "-0,00 € – 0,00 €"},
      {start: -0, end: 0.1e-3, result: "-0,00 € – 0,00 €"},
      {start: -0, end: "0.1e-3", result: "-0,00 € – 0,00 €"},
      {start: "-0", end: 0.1e-3, result: "-0,00 € – 0,00 €"},
      {start: -0, end: -0, result: "≈-0,00 €"},

      // Values starting at negative infinity.
      {start: -Infinity, end: -Infinity, result: "≈-∞ €"},
      {start: -Infinity, end: -0, result: "-∞–0,00 €"},
      {start: -Infinity, end: +0, result: "-∞ € – 0,00 €"},
      {start: -Infinity, end: +Infinity, result: "-∞ € – ∞ €"},

      // Values ending at positive infinity.
      {start: Infinity, end: Infinity, result: "≈∞ €"},

      // Non-special cases.
      {start: 1, end: 100, result: "1,00–100,00 €"},
      {start: -100, end: 100, result: "-100,00 € – 100,00 €"},
      {start: -1000, end: -100, result: "-1.000,00–100,00 €"},
      {start: Math.PI, end: 123_456.789, result: "3,14–123.456,79 €"},
      {start: -Math.PI, end: Math.E, result: "-3,14 € – 2,72 €"},
      {
        start: Number.MAX_SAFE_INTEGER,
        end: 9007199254740993,
        result: "9.007.199.254.740.991,00–9.007.199.254.740.992,00 €",
      },
      {
        start: Number.MAX_SAFE_INTEGER,
        end: "9007199254740993",
        result: "9.007.199.254.740.991,00–9.007.199.254.740.993,00 €",
      },
    ],
  },
  "fr": {
    options: {style: "unit", unit: "meter"},
    ranges: [
      // Values around zero.
      {start: 0, end: 0, result: "≈0 m"},
      {start: -0, end: 0, result: "-0 – 0 m"},
      {start: -0, end: 0.1e-3, result: "-0 – 0 m"},
      {start: -0, end: "0.1e-3", result: "-0 – 0 m"},
      {start: "-0", end: 0.1e-3, result: "-0 – 0 m"},
      {start: -0, end: -0, result: "≈-0 m"},

      // Values starting at negative infinity.
      {start: -Infinity, end: -Infinity, result: "≈-∞ m"},
      {start: -Infinity, end: -0, result: "-∞ – -0 m"},
      {start: -Infinity, end: +0, result: "-∞ – 0 m"},
      {start: -Infinity, end: +Infinity, result: "-∞ – ∞ m"},

      // Values ending at positive infinity.
      {start: Infinity, end: Infinity, result: "≈∞ m"},

      // Non-special cases.
      {start: 1, end: 100, result: "1–100 m"},
      {start: -100, end: 100, result: "-100 – 100 m"},
      {start: -1000, end: -100, result: "-1 000 – -100 m"},
      {start: Math.PI, end: 123_456.789, result: "3,142–123 456,789 m"},
      {start: -Math.PI, end: Math.E, result: "-3,142 – 2,718 m"},
      {
        start: Number.MAX_SAFE_INTEGER,
        end: 9007199254740993,
        result: "9 007 199 254 740 991–9 007 199 254 740 992 m",
      },
      {
        start: Number.MAX_SAFE_INTEGER,
        end: "9007199254740993",
        result: "9 007 199 254 740 991–9 007 199 254 740 993 m",
      },
    ],
  },
  // Non-ASCII digits.
  "ar": {
    options: {},
    ranges: [
      {start: -2, end: -1, result: "؜-٢–١"},
      {start: -1, end: -1, result: "~؜-١"},
      {start: -1, end: 0, result: "؜-١ – ٠"},
      {start: 0, end: 0, result: "~٠"},
      {start: 0, end: 1, result: "٠–١"},
      {start: 1, end: 1, result: "~١"},
      {start: 1, end: 2, result: "١–٢"},
    ],
  },
  "th-u-nu-thai": {
    options: {},
    ranges: [
      {start: -2, end: -1, result: "-๒ - -๑"},
      {start: -1, end: -1, result: "~-๑"},
      {start: -1, end: 0, result: "-๑ - ๐"},
      {start: 0, end: 0, result: "~๐"},
      {start: 0, end: 1, result: "๐-๑"},
      {start: 1, end: 1, result: "~๑"},
      {start: 1, end: 2, result: "๑-๒"},
    ],
  },
  // Approximation sign may consist of multiple characters.
  "no": {
    options: {},
    ranges: [
      {start: 1, end: 1, result: "ca. 1"},
    ],
  },
  // Approximation sign can be a word.
  "ja": {
    options: {},
    ranges: [
      {start: 1, end: 1, result: "約 1"},
    ],
  },
};

for (let [locale, {options, ranges}] of Object.entries(tests)) {
  let nf = new Intl.NumberFormat(locale, options);
  for (let {start, end, result} of ranges) {
    assertEq(nf.formatRange(start, end), result, `${start}-${end}`);
    assertEq(nf.formatRangeToParts(start, end).reduce((acc, part) => acc + part.value, ""),
             result, `${start}-${end}`);
  }
}

{
  const errorTests = [
    // Throws an error if either value is NaN.
    {start: NaN, end: NaN},
    {start: 0, end: NaN},
    {start: NaN, end: 0},
    {start: Infinity, end: NaN},
    {start: NaN, end: Infinity},

    // Positive infinity is larger than any other value.
    {start: Infinity, end: Number.MAX_VALUE},
    {start: Infinity, end: 0},
    {start: Infinity, end: -0},
    {start: Infinity, end: -Number.MAX_VALUE},
    {start: Infinity, end: -Infinity},

    // Negative infinity is smaller than any other value.
    {start: -Number.MAX_VALUE, end: -Infinity},
    {start: -0, end: -Infinity},
    {start: 0, end: -Infinity},
    {start: Number.MAX_VALUE, end: -Infinity},

    // Negative zero is larger than any other negative value.
    {start: -0, end: -0.1},
    {start: -0, end: -Number.MAX_VALUE},
    {start: -0, end: -Infinity},

    // When values are mathematical values, |start| mustn't be larger than |end|.
    {start: 1, end: 0},
    {start: 0, end: -1},
    {start: 1, end: -1},
    {start: -1, end: -2},

    // Positive zero is larger than negative zero.
    {start: 0, end: -0},

    // String cases require to parse and interpret the inputs.
    {start: "10e2", end: "1e-3"},
    {start: "0x100", end: "1e1"},
    {start: ".1e-999999", end: ".01e-999999"},
    {start: ".1e99999", end: "0"},
    // Number.MAX_VALUE is 1.7976931348623157e+308.
    {start: "1.7976931348623158e+308", end: Number.MAX_VALUE},
    // Number.MIN_VALUE is 5e-324.
    {start: "6e-324", end: Number.MIN_VALUE},
  ];

  let nf = new Intl.NumberFormat("en");
  for (let {start, end} of errorTests) {
    assertThrowsInstanceOf(() => nf.formatRange(start, end), RangeError);
    assertThrowsInstanceOf(() => nf.formatRangeToParts(start, end), RangeError);
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
